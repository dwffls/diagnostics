#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

// Include the functions to test
#include "diagnostic_remote_logging/influx_line_protocol.hpp" // Replace with the actual header file name

diagnostic_msgs::msg::KeyValue createKeyValue(const std::string& key, const std::string& value)
{
  diagnostic_msgs::msg::KeyValue output;
  output.key   = key;
  output.value = value;
  return output;
}

// Test toInfluxTimestamp
TEST(InfluxTimestampTests, CorrectConversion)
{
  rclcpp::Time time(1672531200, 123456789); // Example time
  std::string  expected = "1672531200123456789";
  EXPECT_EQ(toInfluxTimestamp(time), expected);
}

// Test escapeSpace
TEST(EscapeSpaceTests, HandlesSpaces)
{
  EXPECT_EQ(escapeSpace("test string"), "test\\ string");
  EXPECT_EQ(escapeSpace("no_space"), "no_space");
  EXPECT_EQ(escapeSpace("multiple spaces here"), "multiple\\ spaces\\ here");
}

// Test is_number
TEST(IsNumberTests, ValidatesNumbers)
{
  EXPECT_TRUE(is_number("123"));
  EXPECT_TRUE(is_number("123.456"));
  EXPECT_TRUE(is_number("-123.456"));
  EXPECT_FALSE(is_number("123abc"));
  EXPECT_FALSE(is_number("abc123"));
  EXPECT_FALSE(is_number(""));
}

// Test formatValues
TEST(FormatValuesTests, FormatsKeyValuePairs)
{
  std::vector<diagnostic_msgs::msg::KeyValue> values;
  values.push_back(createKeyValue("key1", "value"));
  values.push_back(createKeyValue("key2", "42"));
  values.push_back(createKeyValue("key3", "-3.14"));
  values.push_back(createKeyValue("key with spaces", "value with spaces"));

  std::string expected = "key1=\"value\",key2=42,key3=-3.14,key\\ with\\ spaces=\"value with spaces\"";
  EXPECT_EQ(formatValues(values), expected);
}

// Test splitHardwareID
TEST(SplitHardwareIDTests, SplitsCorrectly)
{
  EXPECT_EQ(splitHardwareID("node_name"), std::make_pair(std::string("none"), std::string("node_name")));
  EXPECT_EQ(splitHardwareID("/ns/node_name"), std::make_pair(std::string("ns"), std::string("node_name")));
  EXPECT_EQ(splitHardwareID("/ns/prefix/node_name"), std::make_pair(std::string("ns"), std::string("prefix/node_name")));
}

// Test statusToInfluxLineProtocol
TEST(StatusToInfluxLineProtocolTests, FormatsCorrectly)
{
  diagnostic_msgs::msg::DiagnosticStatus status;
  status.hardware_id = "/ns/node_name";
  status.level       = 2;
  status.message     = "Test message";
  status.values.push_back(createKeyValue("key1", "value1"));
  status.values.push_back(createKeyValue("key2", "42"));

  std::string expected = "node_name,ns=ns level=2,message=\"Test message\",key1=\"value1\",key2=42 1672531200123456789\n";
  std::string output;
  statusToInfluxLineProtocol(output, status, "1672531200123456789");

  EXPECT_EQ(output, expected);
}

// Test arrayToInfluxLineProtocol
TEST(ArrayToInfluxLineProtocolTests, HandlesMultipleStatuses)
{
  auto diag_msg          = std::make_shared<diagnostic_msgs::msg::DiagnosticArray>();
  diag_msg->header.stamp = rclcpp::Time(1672531200, 123456789);

  diagnostic_msgs::msg::DiagnosticStatus status1;
  status1.hardware_id = "/ns1/node1";
  status1.level       = 1;
  status1.message     = "First status";
  status1.values.push_back(createKeyValue("keyA", "valueA"));

  diagnostic_msgs::msg::DiagnosticStatus status2;
  status2.hardware_id = "node2";
  status2.level       = 2;
  status2.message     = "Second status";
  status2.values.push_back(createKeyValue("keyB", "42"));

  diag_msg->status = {status1, status2};

  std::string expected = "node1,ns=ns1 level=1,message=\"First status\",keyA=\"valueA\" 1672531200123456789\n"
                         "node2,ns=none level=2,message=\"Second status\",keyB=42 1672531200123456789\n";

  EXPECT_EQ(arrayToInfluxLineProtocol(diag_msg), expected);
}