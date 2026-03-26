#include <cmath>
#include <numeric>
#include <sstream>

#include "modules/csv_order_generator.hpp"
#include "modules/mock_client.hpp"
#include "modules/mpsc_benchmark.hpp"
#include "modules/order_book_benchmark.hpp"
#include "modules/order_generator.hpp"

#include "core/Order.hpp"
#include "core/order_book_factory.hpp"
#include "utils/thread_pinning.hpp"

using namespace hft;

void printUsage()
{
    std::cout << "Usage: orderbook_benchmark [options]\n"
              << "Options:\n"
              << "  --mode <direct|gateway|mpsc>  (default: direct)\n"
              << "  --book <map|array|vector|hybrid|pool|all> (default: map)\n"
              << "  --scenario <name|all>    (default: mixed)\n"
              << "  --csv <filename>         (optional: load orders from CSV)\n"
              << "  --orders <count>         (default: 10000, if no CSV)\n"
              << "  --port <number>          (default: 12345, for gateway mode)\n"
              << "  --producers <count|all>  (default: 4, for mpsc mode; 'all' sweeps 1/2/4/8)\n"
              << "  --runs <count>           (default: 1)\n"
              << "  --csv_out <filename>     (default: results/results.csv)\n"
              << "  --pin-core <id>          (optional: pin benchmark thread in direct mode)\n"
              << "  --list_books             (list all supported order book types and exit)\n"
              << "  --list_scenarios         (list all supported scenarios and exit)\n"
              << "  --help                   (show this help and exit)\n";
}

struct BenchmarkResult
{
    std::string mode;
    std::string book;
    std::string scenario;
    double mean;
    double latencyStdDev = 0.0;
    uint64_t p99;
    double p99StdDev = 0.0;
    uint64_t max;
    double throughput;
    double throughputStdDev = 0.0;

    double dirInsertMean;
    double dirCancelMean;
    double dirLookupMean;
    double dirMatchMean;

    double serverMean;
    uint64_t serverP99;
    uint64_t serverMax;
    double serverNetMean;
    double serverQueMean;
    double serverEngMean;

    // MPSC-specific fields
    int producerCount = 0;
    uint64_t ordersDropped = 0;
    uint64_t peakQueueDepth = 0;
    double mpscQueueMean = 0.0;
    uint64_t mpscQueueP99 = 0;
    double mpscEngineMean = 0.0;
    uint64_t mpscEngineP99 = 0;
};

// Helper for mean and standard deviation
struct Stats
{
    double mean;
    double stddev;
};
Stats calculateStats(const std::vector<double> &values)
{
    if (values.empty())
        return {0.0, 0.0};
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    double sq_sum = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
    double stddev = std::sqrt(std::max(0.0, sq_sum / values.size() - mean * mean));
    return {mean, stddev};
}

std::vector<BenchmarkResult> loadExistingResults(const std::string &filename)
{
    std::vector<BenchmarkResult> results;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return results;
    }

    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string mode, book, scenario, lat_s, lat_sd_s, p99_s, p99_sd_s, max_s, thru_s, thru_sd_s;
        std::string net_s, que_s, eng_s, ins_s, can_s, lkp_s, mtc_s;
        std::string prod_s, drop_s, depth_s, m_que_s, m_p99_s, m_eng_s, m_ep99_s;

        std::getline(ss, mode, ',');
        std::getline(ss, book, ',');
        std::getline(ss, scenario, ',');
        std::getline(ss, lat_s, ',');
        std::getline(ss, lat_sd_s, ',');
        std::getline(ss, p99_s, ',');
        std::getline(ss, p99_sd_s, ',');
        std::getline(ss, max_s, ',');
        std::getline(ss, thru_s, ',');
        std::getline(ss, thru_sd_s, ',');
        std::getline(ss, net_s, ',');
        std::getline(ss, que_s, ',');
        std::getline(ss, eng_s, ',');
        std::getline(ss, ins_s, ',');
        std::getline(ss, can_s, ',');
        std::getline(ss, lkp_s, ',');
        std::getline(ss, mtc_s, ',');
        std::getline(ss, prod_s, ',');
        std::getline(ss, drop_s, ',');
        std::getline(ss, depth_s, ',');
        std::getline(ss, m_que_s, ',');
        std::getline(ss, m_p99_s, ',');
        std::getline(ss, m_eng_s, ',');
        std::getline(ss, m_ep99_s, ',');

        try
        {
            BenchmarkResult res;
            res.mode = mode;
            res.book = book;
            res.scenario = scenario;

            res.mean = lat_s.empty() ? 0.0 : std::stod(lat_s);
            res.latencyStdDev = lat_sd_s.empty() ? 0.0 : std::stod(lat_sd_s);
            res.p99 = p99_s.empty() ? 0 : std::stoull(p99_s);
            res.p99StdDev = p99_sd_s.empty() ? 0.0 : std::stod(p99_sd_s);
            res.max = max_s.empty() ? 0 : std::stoull(max_s);
            res.throughput = thru_s.empty() ? 0.0 : std::stod(thru_s);
            res.throughputStdDev = thru_sd_s.empty() ? 0.0 : std::stod(thru_sd_s);

            if (mode == "gateway")
            {
                res.serverMean = res.mean;
                res.serverP99 = res.p99;
                res.serverMax = res.max;
                res.mean = 0;
                res.p99 = 0;
                res.max = 0;
                res.dirInsertMean = 0;
                res.dirCancelMean = 0;
                res.dirLookupMean = 0;
                res.dirMatchMean = 0;

                if (!net_s.empty())
                    res.serverNetMean = std::stod(net_s);
                if (!que_s.empty())
                    res.serverQueMean = std::stod(que_s);
                if (!eng_s.empty())
                    res.serverEngMean = std::stod(eng_s);
            }
            else
            {
                res.serverMean = 0;
                res.serverP99 = 0;
                res.serverMax = 0;
                res.serverNetMean = 0;
                res.serverQueMean = 0;
                res.serverEngMean = 0;

                if (!ins_s.empty())
                    res.dirInsertMean = std::stod(ins_s);
                if (!can_s.empty())
                    res.dirCancelMean = std::stod(can_s);
                if (!lkp_s.empty())
                    res.dirLookupMean = std::stod(lkp_s);
                if (!mtc_s.empty())
                    res.dirMatchMean = std::stod(mtc_s);
            }

            // Parse MPSC-specific columns
            if (!prod_s.empty())
                res.producerCount = std::stoi(prod_s);
            if (!drop_s.empty())
                res.ordersDropped = std::stoull(drop_s);
            if (!depth_s.empty())
                res.peakQueueDepth = std::stoull(depth_s);
            if (!m_que_s.empty())
                res.mpscQueueMean = std::stod(m_que_s);
            if (!m_p99_s.empty())
                res.mpscQueueP99 = std::stoull(m_p99_s);
            if (!m_eng_s.empty())
                res.mpscEngineMean = std::stod(m_eng_s);
            if (!m_ep99_s.empty())
                res.mpscEngineP99 = std::stoull(m_ep99_s);
            results.push_back(res);
        }
        catch (...)
        {
        }
    }
    return results;
}

void saveResults(const std::string &filename, const std::vector<BenchmarkResult> &results)
{
    std::ofstream outFile(filename, std::ios::trunc);
    if (!outFile.is_open())
    {
        return;
    }

    // Updated header to include P99StdDev
    outFile << "Mode,Book,Scenario,Latency_ns,LatencyStdDev_ns,P99_ns,P99StdDev_ns,Max_ns,Throughput,ThroughputStdDev,"
               "Network_ns,Queue_ns,Engine_ns,Insert_ns,Cancel_ns,Lookup_ns,Match_ns,Producers,Dropped,PeakDepth,"
               "MpscQue_ns,MpscQueP99_ns,MpscEng_ns,MpscEngP99_ns\n";
    for (const auto &res : results)
    {
        double meanLat = (res.mode == "gateway") ? res.serverMean : res.mean;
        uint64_t p99Lat = (res.mode == "gateway") ? res.serverP99 : res.p99;
        uint64_t maxLat = (res.mode == "gateway") ? res.serverMax : res.max;

        outFile << res.mode << "," << res.book << "," << res.scenario << "," << std::fixed << std::setprecision(2)
                << meanLat << "," << res.latencyStdDev << "," << p99Lat << "," << res.p99StdDev << "," << maxLat << ","
                << std::fixed << std::setprecision(2) << res.throughput << "," << res.throughputStdDev << ","
                << res.serverNetMean << "," << res.serverQueMean << "," << res.serverEngMean << "," << res.dirInsertMean
                << "," << res.dirCancelMean << "," << res.dirLookupMean << "," << res.dirMatchMean << ","
                << res.producerCount << "," << res.ordersDropped << "," << res.peakQueueDepth << ","
                << res.mpscQueueMean << "," << res.mpscQueueP99 << "," << res.mpscEngineMean << "," << res.mpscEngineP99
                << "\n";
    }
}

void upsertResult(std::vector<BenchmarkResult> &results, const BenchmarkResult &newRes)
{
    // For MPSC, each producer count is a distinct experiment — include it in the key.
    auto isSameKey = [&](const BenchmarkResult &r)
    {
        bool base = r.mode == newRes.mode && r.book == newRes.book && r.scenario == newRes.scenario;
        if (newRes.mode == "mpsc")
            return base && r.producerCount == newRes.producerCount;
        return base;
    };
    results.erase(std::remove_if(results.begin(), results.end(), isSameKey), results.end());
    results.push_back(newRes);
}

void cleanupDuplicates(std::vector<BenchmarkResult> &results)
{
    std::reverse(results.begin(), results.end());
    std::vector<BenchmarkResult> uniqueResults;
    for (const auto &res : results)
    {
        bool duplicate = false;
        for (const auto &u : uniqueResults)
        {
            if (u.mode == res.mode && u.scenario == res.scenario && u.book == res.book &&
                (res.mode != "mpsc" || u.producerCount == res.producerCount))
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            uniqueResults.push_back(res);
    }
    results = uniqueResults;
    std::reverse(results.begin(), results.end());
}

void printSummaryTable(const std::vector<BenchmarkResult> &allResults, const std::string &csvOut)
{
    if (allResults.empty())
        return;

    // Sort for a professional dissertation-ready table
    auto sortedResults = allResults;
    std::sort(sortedResults.begin(), sortedResults.end(),
              [](const BenchmarkResult &a, const BenchmarkResult &b)
              {
                  if (a.mode != b.mode)
                      return a.mode < b.mode;
                  if (a.scenario != b.scenario)
                      return a.scenario < b.scenario;
                  return a.book < b.book;
              });

    std::cout << "\n" << std::string(187, '=') << "\n";
    std::cout << "GLOBAL PERFORMANCE SUMMARY (all recorded runs)\n";
    std::cout << std::string(187, '=') << "\n";
    std::cout << std::left << std::setw(10) << "Mode" << std::setw(20) << "Scenario" << std::setw(12) << "Book"
              << std::right << std::setw(13) << "Latency(ns)" << std::setw(12) << "Std" << std::setw(12) << "P99(ns)"
              << std::setw(12) << "P99Std" << std::setw(12) << "Max(ns)" << std::setw(15) << "Throughput"
              << std::setw(12) << "Net/Prod" << std::setw(12) << "Que(ns)" << std::setw(12) << "Eng(ns)"
              << std::setw(12) << "Ins(ns)" << std::setw(12) << "Can(ns)" << std::setw(12) << "Lkp(ns)" << std::setw(15)
              << "Match/Drop";
    std::cout << "\n" << std::string(211, '-') << "\n";

    std::string lastModeScenario = "";
    for (const auto &res : sortedResults)
    {
        std::string currentKey = res.mode + res.scenario;
        if (!lastModeScenario.empty() && currentKey != lastModeScenario)
        {
            std::cout << std::string(187, '-') << "\n";
        }

        std::cout << std::left << std::setw(10) << res.mode << std::setw(20) << res.scenario << std::setw(12)
                  << res.book;

        double displayMean = (res.mode == "gateway") ? res.serverMean : res.mean;
        uint64_t displayP99 = (res.mode == "gateway") ? res.serverP99 : res.p99;
        uint64_t displayMax = (res.mode == "gateway") ? res.serverMax : res.max;

        std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(13) << displayMean << std::setw(12)
                  << res.latencyStdDev << std::setw(12) << std::fixed << std::setprecision(0) << (double)displayP99
                  << std::setw(12) << std::fixed << std::setprecision(2) << res.p99StdDev << std::setw(12) << std::fixed
                  << std::setprecision(0) << (double)displayMax << std::fixed << std::setprecision(0) << std::setw(15)
                  << res.throughput;

        if (res.mode == "gateway")
        {
            std::cout << std::setw(12) << std::fixed << std::setprecision(2) << res.serverNetMean << std::setw(12)
                      << res.serverQueMean << std::setw(12) << res.serverEngMean << std::setw(12) << "-"
                      << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(15) << "-";
        }
        else if (res.mode == "mpsc")
        {
            // Prod column: producer count. Que=queue latency, Eng=engine latency, Match=dropped orders
            std::cout << std::setw(12) << std::fixed << std::setprecision(0) << (double)res.producerCount
                      << std::setw(12) << std::setprecision(2) << res.mpscQueueMean << std::setw(12)
                      << res.mpscEngineMean << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(12) << "-"
                      << std::setw(15) << (double)res.ordersDropped;
        }
        else
        {
            std::cout << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(12)
                      << std::fixed << std::setprecision(2) << res.dirInsertMean << std::setw(12) << res.dirCancelMean
                      << std::setw(12) << res.dirLookupMean << std::setw(15) << res.dirMatchMean;
        }
        std::cout << "\n";
        lastModeScenario = currentKey;
    }
    std::cout << std::string(187, '=') << "\n";
    std::cout << "[Note] Latency = Pure Algorithmic Time (Direct) or End-to-End System Time (Gateway)\n";
    if (!csvOut.empty())
    {
        std::cout << "Results saved to: " << csvOut << "\n\n";
    }
}

void runDirectBenchmark(const std::string &currentBook, const std::string &scenario, const std::vector<Order> &orders,
                        int runs, std::vector<BenchmarkResult> &allResults)
{
    auto book = OrderBookFactory::create(currentBook);
    OrderBookBenchmark benchmark(currentBook, std::move(book));

    std::cout << "Running direct benchmark for " << currentBook << " (" << runs << " runs)...\n";

    std::vector<double> latencies, throughputs, p99s;
    std::vector<double> insLat, canLat, lkpLat, mtcLat;
    uint64_t sumMax = 0;

    for (int r = 0; r < runs; ++r)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto stats = benchmark.runScenario(orders, 1, orders.size() / 10);
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(stats.totalStats.mean);
        p99s.push_back(stats.totalStats.p99);
        sumMax += stats.totalStats.max;

        insLat.push_back(stats.insertStats.mean);
        canLat.push_back(stats.cancelStats.mean);
        lkpLat.push_back(stats.lookupStats.mean);
        mtcLat.push_back(stats.matchStats.mean);

        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        if (durationNs > 0)
        {
            throughputs.push_back(orders.size() * 1e9 / durationNs);
        }
    }

    auto latStats = calculateStats(latencies);
    auto p99Stats = calculateStats(p99s);
    auto thrStats = calculateStats(throughputs);

    BenchmarkResult res;
    res.mode = "direct";
    res.book = currentBook;
    res.scenario = scenario;
    res.mean = latStats.mean;
    res.latencyStdDev = latStats.stddev;
    res.p99 = p99Stats.mean;
    res.p99StdDev = p99Stats.stddev;
    res.max = sumMax / runs;
    res.throughput = thrStats.mean;
    res.throughputStdDev = thrStats.stddev;

    res.dirInsertMean = calculateStats(insLat).mean;
    res.dirCancelMean = calculateStats(canLat).mean;
    res.dirLookupMean = calculateStats(lkpLat).mean;
    res.dirMatchMean = calculateStats(mtcLat).mean;

    upsertResult(allResults, res);
    std::cout << "  Mean Latency:   " << std::fixed << std::setprecision(2) << res.mean << " ± " << res.latencyStdDev
              << " ns\n";
}

void runGatewayBenchmark(const std::string &currentBook, const std::string &scenario, const std::vector<Order> &orders,
                         int runs, int port, std::vector<BenchmarkResult> &allResults)
{
    std::cout << "Running gateway benchmark for " << currentBook << " (" << runs << " runs)...\n";

    std::vector<double> latencies, throughputs, p99s;
    std::vector<double> netLats, queLats, engLats;
    uint64_t sumMax = 0;

    for (int r = 0; r < runs; ++r)
    {
        MockClient client("127.0.0.1", port);
        if (!client.connect())
        {
            std::cerr << "Failed to connect to gateway at 127.0.0.1:" << port << "\n";
            return;
        }

        auto startTotal = std::chrono::high_resolution_clock::now();
        bool sendFailed = false;
        for (const auto &order : orders)
        {
            if (!client.sendOrder(order))
            {
                std::cerr << "Failed to send one or more orders to gateway\n";
                sendFailed = true;
                break;
            }
        }

        if (sendFailed)
        {
            client.disconnect();
            return;
        }

        auto sStats = client.requestServerStats(orders.size());
        auto endTotal = std::chrono::high_resolution_clock::now();

        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTotal - startTotal).count();
        if (durationNs > 0)
        {
            throughputs.push_back(orders.size() * 1e9 / durationNs);
        }

        if (sStats)
        {
            latencies.push_back(sStats->mean);
            p99s.push_back(sStats->p99);
            sumMax += sStats->max;
            netLats.push_back(sStats->netMean);
            queLats.push_back(sStats->queMean);
            engLats.push_back(sStats->engMean);
        }
        client.disconnect();
    }

    auto latStats = calculateStats(latencies);
    auto p99Stats = calculateStats(p99s);
    auto thrStats = calculateStats(throughputs);

    BenchmarkResult gwRes;
    gwRes.mode = "gateway";
    gwRes.book = currentBook;
    gwRes.scenario = scenario;
    gwRes.mean = 0; // gateway uses serverMean
    gwRes.latencyStdDev = latStats.stddev;
    gwRes.throughput = thrStats.mean;
    gwRes.throughputStdDev = thrStats.stddev;

    gwRes.serverMean = latStats.mean;
    gwRes.serverP99 = p99Stats.mean;
    gwRes.p99StdDev = p99Stats.stddev;
    gwRes.serverMax = sumMax / runs;
    gwRes.serverNetMean = calculateStats(netLats).mean;
    gwRes.serverQueMean = calculateStats(queLats).mean;
    gwRes.serverEngMean = calculateStats(engLats).mean;

    upsertResult(allResults, gwRes);
    std::cout << "  Mean Server Latency: " << std::fixed << std::setprecision(2) << gwRes.serverMean << " ± "
              << gwRes.latencyStdDev << " ns\n";
}

void runMpscBenchmark(const std::string &currentBook, const std::string &scenario, const std::vector<Order> &orders,
                      int runs, int producerCount, std::vector<BenchmarkResult> &allResults)
{
    std::cout << "Running MPSC benchmark for " << currentBook << " (" << producerCount << " producers, " << runs
              << " runs)...\n";

    std::vector<double> queueLatencies, engineLatencies, throughputs, queueP99s;
    uint64_t sumQueMax = 0, sumEngP99 = 0, sumDropped = 0, sumDepth = 0;

    MpscResult lastRes{}; // To keep some reference for printMpscTable
    for (int r = 0; r < runs; ++r)
    {
        auto book = OrderBookFactory::create(currentBook);
        MpscResult res = MpscBenchmark::run(currentBook, std::move(book), orders, producerCount);

        queueLatencies.push_back(res.queueMeanNs);
        queueP99s.push_back(res.queueP99Ns);
        engineLatencies.push_back(res.engineMeanNs);
        throughputs.push_back(res.throughputOrdersPerSec);

        sumQueMax += res.queueMaxNs;
        sumEngP99 += res.engineP99Ns;
        sumDropped += res.ordersDropped;
        sumDepth += res.peakQueueDepth;
        lastRes = res;
    }

    auto qStats = calculateStats(queueLatencies);
    auto qP99Stats = calculateStats(queueP99s);
    auto eStats = calculateStats(engineLatencies);
    auto tStats = calculateStats(throughputs);

    BenchmarkResult mpscRes;
    mpscRes.mode = "mpsc";
    mpscRes.book = currentBook;
    mpscRes.scenario = scenario;
    mpscRes.producerCount = producerCount;
    mpscRes.mean = qStats.mean;
    mpscRes.latencyStdDev = qStats.stddev;
    mpscRes.p99 = qP99Stats.mean;
    mpscRes.p99StdDev = qP99Stats.stddev;
    mpscRes.max = sumQueMax / runs;
    mpscRes.throughput = tStats.mean;
    mpscRes.throughputStdDev = tStats.stddev;

    mpscRes.ordersDropped = sumDropped / runs;
    mpscRes.peakQueueDepth = sumDepth / runs;
    mpscRes.mpscQueueMean = qStats.mean;
    mpscRes.mpscQueueP99 = qP99Stats.mean;
    mpscRes.mpscEngineMean = eStats.mean;
    mpscRes.mpscEngineP99 = sumEngP99 / runs;

    mpscRes.serverQueMean = qStats.mean;
    mpscRes.serverEngMean = eStats.mean;

    upsertResult(allResults, mpscRes);

    // We'll update lastRes to represent the means for the internal table display
    lastRes.queueMeanNs = qStats.mean;
    lastRes.engineMeanNs = eStats.mean;
    lastRes.throughputOrdersPerSec = tStats.mean;
    lastRes.ordersDropped = sumDropped / runs;

    printMpscTable(currentBook, scenario, {lastRes});
}

int main(int argc, char *argv[])
{
    std::string mode = "direct";
    std::string bookType = "map";
    std::string scenarioName = "mixed";
    std::string csvFile = "";
    std::string csvOut = "results/results.csv";
    size_t orderCount = 10000;
    int port = 12345;
    int runs = 1;
    int pinCore = -1;
    std::string producersArg = "4"; // Default producer count for MPSC mode; accepts 'all' for sweep

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
            mode = argv[++i];
        else if (arg == "--book" && i + 1 < argc)
            bookType = argv[++i];
        else if (arg == "--scenario" && i + 1 < argc)
            scenarioName = argv[++i];
        else if (arg == "--csv" && i + 1 < argc)
            csvFile = argv[++i];
        else if (arg == "--csv_out" && i + 1 < argc)
            csvOut = argv[++i];
        else if (arg == "--runs" && i + 1 < argc)
        {
            try
            {
                runs = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --runs: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--orders" && i + 1 < argc)
        {
            try
            {
                orderCount = std::stoul(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --orders: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--producers" && i + 1 < argc)
        {
            producersArg = argv[++i];
            // Validate: must be 'all' or a positive integer
            if (producersArg != "all")
            {
                try
                {
                    std::stoi(producersArg);
                }
                catch (...)
                {
                    std::cerr << "Error: --producers must be a positive integer or 'all': " << producersArg << "\n";
                    printUsage();
                    return 1;
                }
            }
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            try
            {
                port = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --port: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--pin-core" && i + 1 < argc)
        {
            try
            {
                pinCore = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --pin-core: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--list_books")
        {
            auto types = OrderBookFactory::getSupportedTypes();
            for (size_t i = 0; i < types.size(); ++i)
                std::cout << types[i] << (i == types.size() - 1 ? "" : " ");
            std::cout << "\n";
            return 0;
        }
        else if (arg == "--list_scenarios")
        {
            auto scenarios = OrderGenerator::getSupportedScenarios();
            for (size_t i = 0; i < scenarios.size(); ++i)
                std::cout << scenarios[i] << (i == scenarios.size() - 1 ? "" : " ");
            std::cout << "\n";
            return 0;
        }
        else if (arg == "--help")
        {
            printUsage();
            return 0;
        }
        else
        {
            std::cerr << "Error: Unknown or incomplete option: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    if (mode != "direct" && mode != "gateway" && mode != "mpsc")
    {
        std::cerr << "Error: Invalid --mode value: " << mode << "\n";
        std::cerr << "Valid values are: direct, gateway, mpsc\n";
        printUsage();
        return 1;
    }

    if ((mode == "direct" || mode == "mpsc") && pinCore >= 0)
    {
        if (hft::pinToCore(pinCore))
            std::cout << "Thread successfully pinned to core " << pinCore << "\n";
        else
            std::cerr << "Warning: Failed to pin benchmark thread to core " << pinCore << "\n";
    }

    std::cout << "HFT OrderBook Benchmark (" << mode << " mode, " << bookType << " book)\n";
    std::cout << "==========================================\n";

    std::vector<std::string> targetScenarios;
    if (scenarioName == "all")
    {
        targetScenarios = OrderGenerator::getSupportedScenarios();
    }
    else
    {
        targetScenarios = {scenarioName};
    }

    std::vector<std::string> targetBooks;
    if (bookType == "all")
    {
        targetBooks = OrderBookFactory::getSupportedTypes();
    }
    else
    {
        targetBooks = {bookType};
    }

    OrderGenerator generator(42);

    // Load existing results to show a "Global Summary"
    std::vector<BenchmarkResult> allResults = loadExistingResults(csvOut);
    cleanupDuplicates(allResults);

    for (const auto &currentScenario : targetScenarios)
    {
        std::vector<Order> orders;
        if (!csvFile.empty())
        {
            if (scenarioName == "all")
            {
                std::cout << "Skipping scenario loop: Using fixed CSV file instead.\n";
                orders = CSVOrderGenerator::parse(csvFile);
                // We run it once for the CSV
                if (currentScenario != targetScenarios[0])
                    continue;
            }
            else
            {
                orders = CSVOrderGenerator::parse(csvFile);
            }
        }
        else
        {
            std::cout << "\nScenario: " << currentScenario << "\n";
            std::cout << "------------------------------------------\n";
            orders = generator.generateScenario(currentScenario, orderCount);
        }

        for (const auto &currentBook : targetBooks)
        {
            if (mode == "direct")
                runDirectBenchmark(currentBook, currentScenario, orders, runs, allResults);
            else if (mode == "gateway")
                runGatewayBenchmark(currentBook, currentScenario, orders, runs, port, allResults);
            else if (mode == "mpsc")
            {
                // Determine which producer counts to sweep
                std::vector<int> producerCounts;
                if (producersArg == "all")
                    producerCounts = {1, 2, 4, 8};
                else
                    producerCounts = {std::stoi(producersArg)};

                for (int p : producerCounts)
                    runMpscBenchmark(currentBook, currentScenario, orders, runs, p, allResults);
            }
            else
            {
                std::cerr << "Error: Unsupported mode at dispatch time: " << mode << "\n";
                return 1;
            }
        }
    }

    if (!csvOut.empty())
        saveResults(csvOut, allResults);

    printSummaryTable(allResults, csvOut);

    return 0;
}
