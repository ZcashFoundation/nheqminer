#include "libstratum/StratumClient.h"
#include "json/json_spirit_reader.h"
#include "json/json_spirit_utils.h"
#include "streams.h"
#include "version.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
}

// Only the expensive solver is controlled; parsing, worker handoff and submission are real.
class ControlledSolver : public ISolver
{
    std::mutex mutex;
    std::condition_variable changed;
    bool started = false;
    size_t released = 0;
    std::vector<std::string> inputs;

public:
    void start() override
    {
        std::lock_guard<std::mutex> lock(mutex);
        started = true;
        changed.notify_all();
    }

    void stop() override {}
    std::string getdevinfo() override { return ""; }
    std::string getname() override { return "controlled solver"; }
    SolverType GetType() const override { return SolverType::CPU; }

    void solve(const char* header, unsigned int headerLength,
        const char*, unsigned int, std::function<bool()>,
        std::function<void(const std::vector<uint32_t>&, size_t, const unsigned char*)> solution,
        std::function<void()> hashDone) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        inputs.emplace_back(header, headerLength);
        const size_t run = inputs.size();
        changed.notify_all();
        // Outlasts the job waits below, so a dropped notification is reported as such.
        check(changed.wait_for(lock, std::chrono::seconds(20), [&] { return released >= run; }),
            "solver call was not released");
        lock.unlock();
        const unsigned char bytes[1344] = {};
        solution({}, sizeof(bytes), bytes);
        hashDone();
    }

    void waitStarted()
    {
        std::unique_lock<std::mutex> lock(mutex);
        check(changed.wait_for(lock, std::chrono::seconds(5), [&] { return started; }),
            "worker did not start");
    }

    void waitForInput(size_t run, const ZcashJob& job)
    {
        std::unique_lock<std::mutex> lock(mutex);
        check(changed.wait_for(lock, std::chrono::seconds(5), [&] { return inputs.size() >= run; }),
            "updated job did not reach the solver");
        CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);
        expected << CEquihashInput(job.header);
        check(inputs[run - 1] == std::string(expected.begin(), expected.end()),
            "solver received the wrong block header for " + job.jobId());
    }

    void release(size_t run)
    {
        std::lock_guard<std::mutex> lock(mutex);
        released = run;
        changed.notify_all();
    }
};

class Jobs
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<ZcashJob> received;

public:
    void record(const ZcashJob* job)
    {
        check(job != nullptr, "unexpected miner disconnect");
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(*job);
        changed.notify_all();
    }

    ZcashJob waitFor(size_t count, const std::string& id, bool clean)
    {
        std::unique_lock<std::mutex> lock(mutex);
        check(changed.wait_for(lock, std::chrono::seconds(5), [&] { return received.size() >= count; }),
            "mining.notify did not reach the miner: " + id);
        const ZcashJob& job = received[count - 1];
        check(job.jobId() == id && job.cleanJobs() == clean, "wrong job ID or clean flag");
        check(job.serverTarget == UintToArith256(uint256S(std::string(64, 'f'))),
            "share target was not applied to " + id);
        return job;
    }
};

void send(tcp::socket& socket, const std::string& message)
{
    boost::asio::write(socket, boost::asio::buffer(message + "\n"));
}

Object receive(tcp::socket& socket, boost::asio::streambuf& buffer)
{
    boost::asio::read_until(socket, buffer, "\n");
    std::istream input(&buffer);
    std::string line;
    std::getline(input, line);
    Value parsed;
    check(json_spirit::read(line, parsed) && parsed.type() == obj_type, "invalid client JSON");
    return parsed.get_obj();
}

void expectMethod(const Object& request, const std::string& method)
{
    check(find_value(request, "method").get_str() == method, "expected " + method);
}

void notify(tcp::socket& socket, const std::string& id, char parent, char merkle,
    const std::string& time, bool clean)
{
    send(socket, "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"" + id +
        "\",\"04000000\",\"" + std::string(64, parent) + "\",\"" + std::string(64, merkle) +
        "\",\"" + std::string(64, '0') + "\",\"" + time + "\",\"ffff071f\"," +
        (clean ? "true" : "false") + "]}");
}

void expectShare(tcp::socket& socket, boost::asio::streambuf& buffer, const ZcashJob& job)
{
    const Object request = receive(socket, buffer);
    expectMethod(request, "mining.submit");
    const Array& params = find_value(request, "params").get_array();
    check(params.size() == 5 && params[0].get_str() == "worker" &&
        params[1].get_str() == job.jobId() && params[2].get_str() == job.time,
        "share lost its original job ID or time");
    check(params[3].get_str().size() == 56 && params[4].get_str().size() == 2694,
        "share nonce or solution serialization changed");
    send(socket, "{\"id\":" + std::to_string(find_value(request, "id").get_int()) +
        ",\"result\":true,\"error\":null}");
}

} // namespace

int main()
{
    try {
        auto io = std::make_shared<boost::asio::io_service>();
        tcp::acceptor pool(*io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
        ControlledSolver solver;
        ZcashMiner miner({&solver});
        miner.start();
        solver.waitStarted();
        Jobs jobs;
        // Connect after the worker, so recording also acknowledges its pending-work update.
        miner.NewJob.connect([&](const ZcashJob* job) { jobs.record(job); });
        ZcashStratumClient client(io, &miner, "127.0.0.1",
            std::to_string(pool.local_endpoint().port()), "worker", "password", 0, 0);
        miner.onSolutionFound([&](const EquihashSolution& solution, const std::string& jobId) {
            return client.submit(&solution, jobId);
        });
        tcp::socket socket(*io);
        pool.accept(socket);
        boost::asio::streambuf buffer;
        expectMethod(receive(socket, buffer), "mining.subscribe");
        send(socket, "{\"id\":1,\"result\":[[],\"00000000\"],\"error\":null}");
        expectMethod(receive(socket, buffer), "mining.authorize");
        send(socket, "{\"id\":2,\"result\":true,\"error\":null}");
        expectMethod(receive(socket, buffer), "mining.extranonce.subscribe");
        send(socket, "{\"id\":3,\"result\":true,\"error\":null}");
        send(socket, "{\"id\":null,\"method\":\"mining.set_target\",\"params\":[\"" +
            std::string(64, 'f') + "\"]}");

        notify(socket, "initial", '1', '2', "01000000", true);
        const ZcashJob initial = jobs.waitFor(1, "initial", true);
        solver.waitForInput(1, initial);
        notify(socket, "refresh-one", '1', '3', "02000000", false);
        jobs.waitFor(2, "refresh-one", false);
        notify(socket, "refresh-two", '1', '4', "03000000", false);
        const ZcashJob refreshed = jobs.waitFor(3, "refresh-two", false);
        check(initial.header.hashPrevBlock == refreshed.header.hashPrevBlock &&
            initial.header.hashMerkleRoot != refreshed.header.hashMerkleRoot,
            "refresh must change the work without changing its parent");
        // Finish the original solve after its job has fallen out of the client's two-job history.
        solver.release(1);
        expectShare(socket, buffer, initial);
        solver.waitForInput(2, refreshed);
        solver.release(2);
        expectShare(socket, buffer, refreshed);

        // A clean next-parent update still replaces work at the next solver-call boundary.
        solver.waitForInput(3, refreshed);
        notify(socket, "next-parent", '5', '6', "04000000", true);
        const ZcashJob next = jobs.waitFor(4, "next-parent", true);
        check(next.header.hashPrevBlock != refreshed.header.hashPrevBlock,
            "clean job must use a new parent");
        solver.release(3);
        expectShare(socket, buffer, refreshed);
        solver.waitForInput(4, next);
        solver.release(4);
        expectShare(socket, buffer, next);
        std::cout << "PASS: non-clean work, in-flight shares and clean updates" << std::endl;
        // Reader and worker threads are intentionally still live; shutdown is not under test.
        std::_Exit(EXIT_SUCCESS);
    } catch (const std::exception& error) {
        check(false, error.what());
    }
}
