#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <print>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include "big_num/internal/add_sub.hpp"
#include "big_num/internal/integer_parse.hpp"

using namespace std::chrono_literals;
using args_t = std::vector<std::string_view>;
using namespace big_num::internal;

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
	auto file = std::ifstream{ std::string(path) };
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
	auto file = std::ofstream(std::string(path), std::ios::out | std::ios::trunc);
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

constexpr auto get_radix(std::string_view num) -> std::uint8_t {
    if (num.starts_with('-') | num.starts_with('+')) num = num.substr(1);
	if (num.starts_with("0b")) return 2;
	if (num.starts_with("0o")) return 8;
	if (num.starts_with("0x")) return 16;
	return 10;
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
	auto a = Integer();
    auto err = parse_integer(a, num);
    if (!err) {
        std::println(stderr, "Error: {}", err.error());
        return;
    }
	auto time = t.end();
	if (file_path) {
        auto n = to_string(a, get_radix(num), { .show_prefix = true });
		write_to_file(*file_path, { n, time });
	} else {
		std::println("benchmark_parse: took {}", time);
	}
}

void benchmark_binary(args_t& args, auto&& fn) {
    if (args.size() == 0) {
        throw std::runtime_error("Please provide at least one argument.");
    }
    auto a = std::string();
    auto b = std::string();
	
	std::optional<std::string_view> file_path{};
	if (args.back() == "-f") {
        args.pop_back();
		auto file = args.back();
		args.pop_back();
		file_path = file;

		auto res = read_file(file);
		if (res.empty()) {
			throw std::runtime_error("No arg found");
		}
        if (res.size() < 2) {
            throw std::runtime_error("Please provide two numbers.");
        }
        a = res[0];
        b = res[1];
	} else {
        if (args.size() < 2) {
            throw std::runtime_error("Please provide two numbers after the flag.");
        }

        a = args.back();
        args.pop_back();
        b = args.back();
        args.pop_back();
    }
    Integer l, r, res;
    auto err = parse_integer(l, a);
    if (!err) {
        std::println(stderr, "Error: {}", err.error());
        exit(1);
    }

    err = parse_integer(r, b);
    if (!err) {
        std::println(stderr, "Error: {}", err.error());
        exit(1);
    }

	Timer t;
    fn(res, l, r);
	auto time = t.end();
	if (file_path) {
        auto n = to_string(res, get_radix(a), { .show_prefix = true });
		write_to_file(*file_path, { n, time });
	} else {
		std::println("benchmark_parse: took {}", time);
	}
}

void parse_args(args_t& args) {
	while (!args.empty()) {
		auto arg = args.back();
		args.pop_back();
		if (arg == "-c") return benchmark_parse(args);
		if (arg == "-a") return benchmark_binary(args,[](auto& res, auto const& l, auto const& r) { add(res, l, r); });
		if (arg == "-s") return benchmark_binary(args,[](auto& res, auto const& l, auto const& r) { sub(res, l, r); });
		if (arg == "-m") return benchmark_binary(args,[](auto& res, auto const& l, auto const& r) { mul(res, l, r); });
	}
}

int main(int argc, char** argv) {
	auto args = init_args(argc, argv); 
	parse_args(args);
	return 0;
}
