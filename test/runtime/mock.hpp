#pragma once

#include <any>
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct MockArg {
	std::any val;

	template <typename T>
	auto as() const -> T {
		return std::any_cast<T>(val);
	}
};

struct MockTest {
	std::string input {};
	std::string output{};
	std::unordered_map<std::string, MockArg> args;


	void add_arg(std::string key, MockArg arg) {
		args[std::move(key)] = std::move(arg);
	}

	template <typename T>
	auto get_arg(std::string_view key) const -> T {
		auto it = args.find(std::string(key));
		if (it == args.end()) throw std::runtime_error(std::format("Unknown key: {}", key));
		auto& temp = it->second;
		return temp.as<T>();
	}
}; 

struct Mock {
	std::vector<MockTest> tests;
};
