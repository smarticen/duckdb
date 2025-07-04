#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "sqlite/sqllogic_test_logger.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

namespace duckdb {
// bool FailureSummary::summarize_failures = false;
// size_t FailureSummary::failures_summary_counter = 0;
// vector<string> FailureSummary::failures_summary;
// mutex FailureSummary::counter_mutex;
// mutex FailureSummary::summary_mutex;

// FailureSummary::FailureSummary() = default;
// FailureSummary::~FailureSummary() = default;

// FailureSummary::FailureSummary() {
// }

// FailureSummary::~FailureSummary() {
// }

FailureSummary &FailureSummary::Instance() {
	static FailureSummary instance;
	return instance;
}

string FailureSummary::GetFailureSummary() {
	lock_guard<mutex> guard(summary_mutex);
	std::ostringstream oss;
	for (auto &line : failures_summary) {
		oss << line;
	}
	return oss.str();
}

size_t FailureSummary::GetSummaryCounter() {
	{
		lock_guard<mutex> lock(counter_mutex);
		++failures_summary_counter;
	}
	return failures_summary_counter;
}

bool FailureSummary::SummarizeFailures() {
	return summarize_failures;
}

void FailureSummary::SetSummarizeFailures(bool state) {
	summarize_failures = state;
}

static bool test_force_storage = false;
static bool test_force_reload = false;
static bool test_memory_leaks = false;

bool TestForceStorage() {
	return test_force_storage;
}

bool TestForceReload() {
	return test_force_reload;
}

bool TestMemoryLeaks() {
	return test_memory_leaks;
}

// bool SummarizeFailures() {
// 	return summary.summarize_failures;
// }

} // namespace duckdb

int main(int argc, char *argv[]) {
	auto &summary = FailureSummary::Instance();
	duckdb::unique_ptr<FileSystem> fs = FileSystem::CreateLocal();
	string test_directory = DUCKDB_ROOT_DIRECTORY;

	const char *summarize = std::getenv("SUMMARIZE_FAILURES");
	if (summarize != nullptr && std::string(summarize) == "1") {
		summary.SetSummarizeFailures(true);
	}

	int new_argc = 0;
	auto new_argv = duckdb::unique_ptr<char *[]>(new char *[argc]);
	for (int i = 0; i < argc; i++) {
		if (string(argv[i]) == "--force-storage") {
			test_force_storage = true;
		} else if (string(argv[i]) == "--force-reload" || string(argv[i]) == "--force-restart") {
			test_force_reload = true;
		} else if (StringUtil::StartsWith(string(argv[i]), "--memory-leak") ||
		           StringUtil::StartsWith(string(argv[i]), "--test-memory-leak")) {
			test_memory_leaks = true;
		} else if (string(argv[i]) == "--test-dir") {
			test_directory = string(argv[++i]);
		} else if (string(argv[i]) == "--test-temp-dir") {
			SetDeleteTestPath(false);
			auto test_dir = string(argv[++i]);
			if (fs->DirectoryExists(test_dir)) {
				fprintf(stderr, "--test-temp-dir cannot point to a directory that already exists (%s)\n",
				        test_dir.c_str());
				return 1;
			}
			SetTestDirectory(test_dir);
		} else if (string(argv[i]) == "--require") {
			AddRequire(string(argv[++i]));
		} else if (string(argv[i]) == "--zero-initialize") {
			SetDebugInitialize(0);
		} else if (string(argv[i]) == "--one-initialize") {
			SetDebugInitialize(0xFF);
		} else if (string(argv[i]) == "--single-threaded") {
			SetSingleThreaded();
		} else if (string(argv[i]) == "--summarize-failures") {
			summary.SetSummarizeFailures(true);
		} else {
			new_argv[new_argc] = argv[i];
			new_argc++;
		}
	}

	TestChangeDirectory(test_directory);
	// delete the testing directory if it exists
	auto dir = TestCreatePath("");
	try {
		TestDeleteDirectory(dir);
		// create the empty testing directory
		TestCreateDirectory(dir);
	} catch (std::exception &ex) {
		fprintf(stderr, "Failed to create testing directory \"%s\": %s\n", dir.c_str(), ex.what());
		return 1;
	}

	RegisterSqllogictests();
	int result = Catch::Session().run(new_argc, new_argv.get());

	std::string failures_summary = summary.GetFailureSummary();
	if (!failures_summary.empty() && summary.summarize_failures) {
		std::cerr << "\n====================================================" << std::endl;
		std::cerr << "================  FAILURES SUMMARY  ================" << std::endl;
		std::cerr << "====================================================\n" << std::endl;
		std::cerr << failures_summary;
	}

	if (DeleteTestPath()) {
		TestDeleteDirectory(dir);
	}

	return result;
}
