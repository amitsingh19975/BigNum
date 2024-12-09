#include "big_num.hpp"
#include "big_num/basic.hpp"
#include "big_num/basic_integer.hpp"
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
using args_t = std::vector<std::string_view>;
using namespace dark;

struct Timer {
	using base_type = std::chrono::time_point<std::chrono::high_resolution_clock>;
	auto end() -> std::string {
		auto end = std::chrono::high_resolution_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
		return std::format("{}", diff);
	}

private:
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

auto write_to_file(std::string_view path, std::initializer_list<std::string> const& lines) {
	auto file = std::ofstream(path, std::ios::out | std::ios::trunc);
	for (auto it = lines.begin(); it != lines.end(); ++it) {
		auto const& line = *it;
		file << line;
		if (it + 1 < lines.end()) file << '\n';
	}
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

constexpr auto get_radix(std::string_view num) -> Radix {
	if (num.starts_with("0b")) return Radix::Binary;
	if (num.starts_with("0o")) return Radix::Octal;
	if (num.starts_with("0x")) return Radix::Hex;
	return Radix::Dec;
}

void benchmark_parse(args_t& args) {
	if (args.size() == 0) {
		throw std::runtime_error("Please provide a number after '-c' arg.");
	}

	auto num = std::string(args.back());
	args.pop_back();
	
	std::optional<std::string_view> file_path{};
	if (num == "-f") {
		auto file = args.back();
		args.pop_back();
		file_path = file;

		auto res = read_file(file);
		if (res.empty()) {
			throw std::runtime_error("No arg found");
		}
		num = res.back();
	}

	Timer t;
	auto a = dark::BigInteger(num);
	auto time = t.end();
	if (file_path) {
		auto res = a.to_str(get_radix(num));
		write_to_file(*file_path, { a.to_str(get_radix(num), true), time });
	} else {
		std::println("benchmark_parse: took {}", time);
	}
}

void parse_args(args_t& args) {
	while (!args.empty()) {
		auto arg = args.back();
		args.pop_back();
		if (arg == "-c") return benchmark_parse(args);
	}
}

int main(int argc, char** argv) {
	auto args = init_args(argc, argv); 
	parse_args(args);
	return 0;
}
