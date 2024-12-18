#include "diagnostic_remote_logging/telegraf.hpp"

Telegraf::Telegraf()
    : Node("telegraf")
{
  telegraf_url_ = this->declare_parameter<std::string>("telegraf_url", "http://localhost:8186/telegraf");

  if (declare_parameter("send_agg", true)) {
    diag_sub_ = this->create_subscription<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics_agg", rclcpp::SensorDataQoS(), std::bind(&Telegraf::diagnosticsCallback, this, std::placeholders::_1));
  }

  if (declare_parameter<bool>("send_top_level_state", true)) {
    top_level_sub_ = this->create_subscription<diagnostic_msgs::msg::DiagnosticStatus>("/diagnostics_toplevel_state", rclcpp::SensorDataQoS(), std::bind(&Telegraf::topLevelCallback, this, std::placeholders::_1));
  }

  setupConnection(telegraf_url_);
}

void Telegraf::diagnosticsCallback(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg)
{
  std::string output = arrayToInfluxLineProtocol(msg);

  if (!sendToTelegraf(output)) {
    RCLCPP_ERROR(this->get_logger(), "Failed to send /diagnostics_agg to telegraf");
  }

  // RCLCPP_INFO(this->get_logger(), "%s", output.c_str());
}

void Telegraf::topLevelCallback(const diagnostic_msgs::msg::DiagnosticStatus::SharedPtr msg)
{
  std::string output = topLevelToInfluxLineProtocol(msg, this->get_clock()->now());

  if (!sendToTelegraf(output)) {
    RCLCPP_ERROR(this->get_logger(), "Failed to send /diagnostics_toplevel_state to telegraf");
  }

  // RCLCPP_INFO(this->get_logger(), "%s", output.c_str());
}

void Telegraf::setupConnection(const std::string& url)
{
  curl_global_init(CURL_GLOBAL_ALL);
  curl_ = curl_easy_init();
  if (!curl_) {
    throw std::runtime_error("Failed to initialize curl");
  }

  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1); // Use HTTP/1.1 for keep-alive
  curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);                 // Set timeout as needed
  curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);                   // Enable TCP keep-alive
}

bool Telegraf::sendToTelegraf(const std::string& data)
{
  if (!curl_) {
    RCLCPP_ERROR(this->get_logger(), "cURL not initialized.");
    return false;
  }

  curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());

  // Perform the request
  CURLcode res = curl_easy_perform(curl_);

  // Check for errors
  if (res != CURLE_OK) {
    RCLCPP_ERROR(this->get_logger(), "cURL error: %s", curl_easy_strerror(res));
    return false;
  }
  long response_code;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code != 204) {
    RCLCPP_ERROR(this->get_logger(), "Error (%ld) when sending to telegraf:\n%s", response_code, data.c_str());
    return false;
  }

  return true;
}

Telegraf::~Telegraf()
{
  if (curl_) {
    curl_easy_cleanup(curl_);
  }
  curl_global_cleanup();
}

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Telegraf>());
  rclcpp::shutdown();
  return 0;
}