#include "diagnostic_remote_logging/influxdb.hpp"

InfluxDB::InfluxDB(const rclcpp::NodeOptions &opt)
    : Node("influxdb", opt)
{
  post_url_ = this->declare_parameter<std::string>("connection.url", "http://localhost:8086/api/v2/write");

  if (post_url_.empty())
  {
    throw std::runtime_error("Parameter connection.url must be set");
  }

  std::string organization = declare_parameter("connection.organization", "");
  std::string bucket = declare_parameter("connection.bucket", "");
  influx_token_ = declare_parameter("connection.token", "");

  // Check if any of the parameters is set
  if (!organization.empty() || !bucket.empty() || !influx_token_.empty())
  {
    // Ensure all parameters are set
    if (organization.empty() || bucket.empty() || influx_token_.empty())
    {
      throw std::runtime_error("All parameters (connection.organization, connection.bucket, connection.token) must be set, or when using a proxy like Telegraf none have to be set.");
    }

    // Construct the Telegraf URL
    post_url_ += "?org=" + organization;
    post_url_ += "&bucket=" + bucket;
  }

  setupConnection(post_url_);

  if (declare_parameter("send.agg", true))
  {
    diag_sub_ = this->create_subscription<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics_agg", rclcpp::SensorDataQoS(), std::bind(&InfluxDB::diagnosticsCallback, this, std::placeholders::_1));
  }

  if (declare_parameter<bool>("send.top_level_state", true))
  {
    top_level_sub_ = this->create_subscription<diagnostic_msgs::msg::DiagnosticStatus>("/diagnostics_toplevel_state", rclcpp::SensorDataQoS(), std::bind(&InfluxDB::topLevelCallback, this, std::placeholders::_1));
  }
}

void InfluxDB::diagnosticsCallback(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg)
{
  std::string output = arrayToInfluxLineProtocol(msg);

  if (!sendToInfluxDB(output))
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to send /diagnostics_agg to telegraf");
  }

  // RCLCPP_INFO(this->get_logger(), "%s", output.c_str());
}

void InfluxDB::topLevelCallback(const diagnostic_msgs::msg::DiagnosticStatus::SharedPtr msg)
{
  std::string output = topLevelToInfluxLineProtocol(msg, this->get_clock()->now());

  if (!sendToInfluxDB(output))
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to send /diagnostics_toplevel_state to telegraf");
  }

  // RCLCPP_INFO(this->get_logger(), "%s", output.c_str());
}

void InfluxDB::setupConnection(const std::string &url)
{
  curl_global_init(CURL_GLOBAL_ALL);
  curl_ = curl_easy_init();
  if (!curl_)
  {
    throw std::runtime_error("Failed to initialize curl");
  }

  struct curl_slist *headers = nullptr;

  if (!influx_token_.empty()){
    headers = curl_slist_append(headers, ("Authorization: Token " + influx_token_).c_str());
  }

  headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
  headers = curl_slist_append(headers, "Accept: application/json");

  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1); // Use HTTP/1.1 for keep-alive
  curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);                 // Set timeout as needed
  curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);                   // Enable TCP keep-alive
  curl_easy_setopt(curl_, CURLOPT_POST, 1L);
}

bool InfluxDB::sendToInfluxDB(const std::string &data)
{
  if (!curl_)
  {
    RCLCPP_ERROR(this->get_logger(), "cURL not initialized.");
    return false;
  }

  curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());

  // Perform the request
  CURLcode res = curl_easy_perform(curl_);

  // Check for errors
  if (res != CURLE_OK)
  {
    RCLCPP_ERROR(this->get_logger(), "cURL error: %s", curl_easy_strerror(res));
    return false;
  }
  long response_code;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code != 204)
  {
    RCLCPP_ERROR(this->get_logger(), "Error (%ld) when sending to telegraf:\n%s", response_code, data.c_str());
    return false;
  }

  return true;
}

InfluxDB::~InfluxDB()
{
  if (curl_)
  {
    curl_easy_cleanup(curl_);
  }
  curl_global_cleanup();
}

#include "rclcpp_components/register_node_macro.hpp"

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(InfluxDB)