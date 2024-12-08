#include "big_num.hpp"
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
using args_t = std::vector<std::string_view>;

struct Timer {
	using base_type = std::chrono::time_point<std::chrono::high_resolution_clock>;
	Timer(std::string name)
		: m_name(std::move(name))
	{}
	Timer(Timer const&) = default;
	Timer& operator=(Timer const&) = default;
	Timer(Timer &&) = default;
	Timer& operator=(Timer &&) = default;
	
	~Timer() {
		auto end = std::chrono::high_resolution_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
		std::println("{}: it took {}", m_name, diff);
	}

private:
	std::string m_name;
	base_type m_start{std::chrono::high_resolution_clock::now()};
};

auto read_file(std::string_view path) -> std::vector<std::string> {
	auto res = std::vector<std::string>{};
	auto file = std::ifstream(path);
	if (!file) {
		throw std::runtime_error(std::format("Unable to open file: '{}'", path));
	}

	auto line = std::string();
	while (std::getline(file, line)) {
		res.push_back(std::move(line));
		line.clear();
	}
	return res;
}

args_t init_args(int argc, char** argv) {
	auto size = static_cast<std::size_t>(argc - 1);
	if (size == 0) {
		throw std::runtime_error("Args cannot be empty.");
	}
	args_t res;
	res.reserve(size);

	for (auto i = 0zu; i < size; ++i) {
		res.push_back(argv[i + 1]);
	}


	std::reverse(res.begin(), res.end());
	return res;
}

void benchmark_construction(args_t& args) {
	if (args.size() == 0) {
		throw std::runtime_error("Please provide a number after '-c' arg.");
	}

	auto num = std::string(args.back());
	args.pop_back();

	if (num == "-f") {
		auto file = args.back();
		args.pop_back();

		auto res = read_file(file);
		if (res.empty()) {
			throw std::runtime_error("No arg found");
		}
		num = res.back();
	}

	Timer t("benchmark_construction");
	[[maybe_unused]] volatile auto a = dark::BigInteger(num); // volatile is used to prevent the compiler from deleting this code.
}

void parse_args(args_t& args) {
	while (!args.empty()) {
		auto arg = args.back();
		args.pop_back();
		if (arg == "-c") return benchmark_construction(args);
	}
}

int main(int argc, char** argv) {
	auto args = init_args(argc, argv); 
	parse_args(args);
	return 0;
}
