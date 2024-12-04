#pragma once

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "rclcpp/rclcpp.hpp"

std::string toInfluxTimestamp(const rclcpp::Time& time)
{
  double timestamp = time.seconds();
  // Extract the integer part (seconds)
  uint64_t seconds = static_cast<uint64_t>(timestamp);
  // Extract the fractional part (nanoseconds) by subtracting the integer part and scaling
  uint64_t nanoseconds = static_cast<uint64_t>((timestamp - seconds) * 1e9);

  // Convert both parts to strings
  std::string secStr     = std::to_string(seconds);
  std::string nanosecStr = std::to_string(nanoseconds);

  // Ensure the nanoseconds part is zero-padded to 9 digits
  nanosecStr = std::string(9 - nanosecStr.length(), '0') + nanosecStr;

  // Concatenate and return the result
  return secStr + nanosecStr;
}

std::string escapeSpace(const std::string& input)
{
  std::string result;
  for (char c : input) {
    if (c == ' ') {
      result += '\\'; // Add a backslash before the space
    }
    result += c; // Add the original character
  }
  return result;
}

std::string formatValues(const std::vector<diagnostic_msgs::msg::KeyValue>& values)
{
  std::string formatted;
  for (const auto& kv : values) {
    formatted += escapeSpace(kv.key) + "=" + escapeSpace(kv.value) + ",";
  }
  if (!formatted.empty()) {
    formatted.pop_back(); // Remove the last comma
  }
  return formatted;
}

std::pair<std::string, std::string> splitHardwareID(const std::string& input)
{
  size_t first_slash_pos = input.find('/');

  // If no slash is found, treat the entire input as the node_name
  if (first_slash_pos == std::string::npos) {
    return {"\"\"", input};
  }

  size_t second_slash_pos = input.find('/', first_slash_pos + 1);

  // If the second slash is found, extract the "ns" and "node" parts
  if (second_slash_pos != std::string::npos) {
    std::string ns   = input.substr(first_slash_pos + 1, second_slash_pos - first_slash_pos - 1);
    std::string node = input.substr(second_slash_pos + 1);
    return {ns, node};
  }

  // If no second slash is found, everything after the first slash is the node
  std::string node = input.substr(first_slash_pos + 1);
  return {"\"\"", node}; // ns is empty, node is the remaining string
}

void statusToInfluxLineProtocol(std::string& output, const diagnostic_msgs::msg::DiagnosticStatus& status, const std::string& timestamp_str)
{
  // hardware_id is empty for analyzer groups, so skip them
  if (status.hardware_id.empty()) {
    return;
  }

  auto [ns, identifier] = splitHardwareID(status.hardware_id);
  output += identifier + ",ns=" + ns + " level=" + std::to_string(status.level) + ",message=\"" + status.message + "\"";
  if (status.values.size()) {
    output += "," + formatValues(status.values);
  }
  output += " " + timestamp_str + "\n";
}

std::string topLevelToInfluxLineProtocol(const diagnostic_msgs::msg::DiagnosticStatus::SharedPtr& msg, const rclcpp::Time& time)
{
  std::string output = msg->name + " level=" + std::to_string(msg->level) + " " + toInfluxTimestamp(time) + "\n";
  return output;
};

std::string arrayToInfluxLineProtocol(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr& diag_msg)
{
  std::string output;
  std::string timestamp = toInfluxTimestamp(diag_msg->header.stamp);

  for (auto& status : diag_msg->status) {
    statusToInfluxLineProtocol(output, status, timestamp);
  }

  return output;
};