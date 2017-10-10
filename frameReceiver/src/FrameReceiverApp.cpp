/**
 * FrameReceiverApp.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: Tim Nicholls, STFC Application Engineering Group
 */

#include <signal.h>
#include <iostream>
#include <fstream>
using namespace std;

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "logging.h"
#include "FrameReceiverApp.h"

using namespace FrameReceiver;

boost::shared_ptr<FrameReceiverController> FrameReceiverApp::controller_;

IMPLEMENT_DEBUG_LEVEL;

#ifndef BUILD_DIR
#define BUILD_DIR "."
#endif

static bool has_suffix(const std::string &str, const std::string &suffix)
{
  return str.size() >= suffix.size() &&
      str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

//! Constructor for FrameReceiverApp class.
//!
//! This constructor initialises the FrameReceiverApp instance

FrameReceiverApp::FrameReceiverApp(void)
{

  // Retrieve a logger instance
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());
  logger_ = Logger::getLogger("FR.App");

  // Instantiate a controller
  controller_ = boost::shared_ptr<FrameReceiverController>(
      new FrameReceiverController()
  );

}

//! Destructor for FrameReceiverApp class
FrameReceiverApp::~FrameReceiverApp()
{
  controller_.reset();
}

//! Parse command-line arguments and configuration file options.
//!
//! This method parses command-line arguments and configuration file options
//! to configure the application for operation. Most options can either be
//! given at the command line or stored in an INI-formatted configuration file.
//! The configuration options are stored in the FrameReceiverConfig helper object for
//! retrieval throughout the application.
//!
//! \param argc - standard command-line argument count
//! \param argv - array of char command-line options
//! \return return code, 0 if OK, 1 if option parsing failed

int FrameReceiverApp::parse_arguments(int argc, char** argv)
{

  int rc = 0;
  try
  {
    std::string config_file;

    // Declare a group of options that will allowed only on the command line
    po::options_description generic("Generic options");
    generic.add_options()
        ("help,h",
         "Print this help message")
        ("version,v",
         "Print program version string")
        ("config,c",    po::value<string>(&config_file),
         "Specify program configuration file")
        ;

    // Declare a group of options that will be allowed both on the command line
    // and in the configuration file
    po::options_description config("Configuration options");
    config.add_options()
        ("debug,d",      po::value<unsigned int>()->default_value(debug_level),
        "Set the debug level")
    ("node,n",       po::value<unsigned int>()->default_value(FrameReceiver::Defaults::default_node),
        "Set the frame receiver node ID")
    ("logconfig,l",  po::value<string>(),
        "Set the log4cxx logging configuration file")
        ("maxmem,m",     po::value<std::size_t>()->default_value(FrameReceiver::Defaults::default_max_buffer_mem),
         "Set the maximum amount of shared memory to allocate for frame buffers")
        ("sensortype,s", po::value<std::string>()->default_value("unknown"),
         "Set the sensor type to receive frame data from")
        ("path",         po::value<std::string>()->default_value(""),
         "Path to load the decoder library from")
        ("rxtype",       po::value<std::string>()->default_value("udp"),
         "Set the interface to use for receiving frame data (udp or zmq)")
        ("port,p",       po::value<std::string>()->default_value(FrameReceiver::Defaults::default_rx_port_list),
         "Set the port to receive frame data on")
        ("ipaddress,i",  po::value<std::string>()->default_value(FrameReceiver::Defaults::default_rx_address),
         "Set the IP address of the interface to receive frame data on")
        ("sharedbuf",    po::value<std::string>()->default_value(FrameReceiver::Defaults::default_shared_buffer_name),
         "Set the name of the shared memory frame buffer")
        ("frametimeout", po::value<unsigned int>()->default_value(FrameReceiver::Defaults::default_frame_timeout_ms),
        "Set the incomplete frame timeout in ms")
    ("frames,f",     po::value<unsigned int>()->default_value(FrameReceiver::Defaults::default_frame_count),
        "Set the number of frames to receive before terminating")
    ("packetlog",    po::value<bool>()->default_value(FrameReceiver::Defaults::default_enable_packet_logging),
        "Enable logging of packet diagnostics to file")
        ("rxbuffer",     po::value<unsigned int>()->default_value(FrameReceiver::Defaults::default_rx_recv_buffer_size),
        "Set UDP receive buffer size")
    ("ctrl",         po::value<std::string>()->default_value(FrameReceiver::Defaults::default_ctrl_chan_endpoint),
        "Set the control channel endpoint")
        ("ready",        po::value<std::string>()->default_value(FrameReceiver::Defaults::default_frame_ready_endpoint),
         "Set the frame ready channel endpoint")
        ("release",      po::value<std::string>()->default_value(FrameReceiver::Defaults::default_frame_release_endpoint),
         "Set the frame release channel endpoint")
        ;

    // Group the variables for parsing at the command line and/or from the configuration file
    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config);
    po::options_description config_file_options;
    config_file_options.add(config);

    // Parse the command line options
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    // If the command-line help option was given, print help and exit
    if (vm.count("help"))
    {
      std::cout << "usage: frameReceiver [options]" << std::endl << std::endl;
      std::cout << cmdline_options << std::endl;
      return 1;
    }

    // If the command line version option was given, print version and exit
    if (vm.count("version"))
    {
      std::cout << "Will print version here" << std::endl;
      return 1;
    }

    if (vm.count("debug"))
    {
      set_debug_level(vm["debug"].as<unsigned int>());
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Debug level set to  " << debug_level);
    }
    // If the command line config option was given, parse the specified configuration
    // file for additional options. Note that boost::program_options gives precedence
    // to the first instance occurring. In this case, that implies that command-line
    // options have precedence over equivalent configuration file entries.
    if (vm.count("config"))
    {
      std::ifstream config_ifs(config_file.c_str());
      if (config_ifs)
      {
        LOG4CXX_DEBUG_LEVEL(1, logger_, "Parsing configuration file " << config_file);
        po::store(po::parse_config_file(config_ifs, config_file_options, true), vm);
        po::notify(vm);
      }
      else
      {
        LOG4CXX_ERROR(logger_, "Unable to open configuration file " << config_file << " for parsing");
        exit(1);
      }
    }

    if (vm.count("logconfig"))
    {
      std::string logconf_fname = vm["logconfig"].as<string>();
      if (has_suffix(logconf_fname, ".xml")) {
        log4cxx::xml::DOMConfigurator::configure(logconf_fname);
      } else {
        PropertyConfigurator::configure(logconf_fname);
      }
      LOG4CXX_DEBUG_LEVEL(1, logger_, "log4cxx config file is set to " << logconf_fname);
    } else {
      BasicConfigurator::configure();
    }

    if (vm.count("maxmem"))
    {
      config_.max_buffer_mem_ = vm["maxmem"].as<std::size_t>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting frame buffer maximum memory size to " << config_.max_buffer_mem_);
    }

    if (vm.count("sensortype"))
    {
      config_.sensor_type_ = vm["sensortype"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting sensor type to " << config_.sensor_type_);
    }

    if (vm.count("path"))
    {
      config_.sensor_path_ = vm["path"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting decoder path to " << config_.sensor_path_);
    }

    if (vm.count("rxtype"))
    {
      std::string rx_name = vm["rxtype"].as<std::string>();
      config_.rx_type_ = config_.map_rx_name_to_type(rx_name);
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting rx type to " << rx_name << " (" << config_.rx_type_ << ")");
    }

    if (vm.count("port"))
    {
      config_.rx_ports_.clear();
      config_.tokenize_port_list(config_.rx_ports_, vm["port"].as<std::string>());

      std::stringstream ss;
      for (std::vector<uint16_t>::iterator itr = config_.rx_ports_.begin(); itr !=config_.rx_ports_.end(); itr++)
      {
        ss << *itr << " ";
      }
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting RX port(s) to " << ss.str());
    }

    if (vm.count("ipaddress"))
    {
      config_.rx_address_ = vm["ipaddress"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting RX interface address to " << config_.rx_address_);
    }

    if (vm.count("sharedbuf"))
    {
      config_.shared_buffer_name_ = vm["sharedbuf"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting shared frame buffer name to " << config_.shared_buffer_name_);
    }

    if (vm.count("frametimeout"))
    {
      config_.frame_timeout_ms_ = vm["frametimeout"].as<unsigned int>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting incomplete frame timeout to " << config_.frame_timeout_ms_);
    }

    if (vm.count("frames"))
    {
      config_.frame_count_ = vm["frames"].as<unsigned int>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting number of frames to receive to " << config_.frame_count_);
    }

    if (vm.count("packetlog"))
    {
      config_.enable_packet_logging_ = vm["packetlog"].as<bool>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Packet diagnostic logging is " <<
                                                                      (config_.enable_packet_logging_ ? "enabled" : "disabled"));
    }

    if (vm.count("rxbuffer"))
    {
      config_.rx_recv_buffer_size_ = vm["rxbuffer"].as<unsigned int>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "RX receive buffer size is " << config_.rx_recv_buffer_size_);
    }

    if (vm.count("ctrl"))
    {
      config_.ctrl_channel_endpoint_ = vm["ctrl"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting control channel endpoint to " << config_.ctrl_channel_endpoint_);
    }

    if (vm.count("ready"))
    {
      config_.frame_ready_endpoint_ = vm["ready"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting frame ready channel endpoint to " << config_.frame_ready_endpoint_);
    }

    if (vm.count("release"))
    {
      config_.frame_release_endpoint_ = vm["release"].as<std::string>();
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Setting frame release channel endpoint to " << config_.frame_release_endpoint_);
    }

  }
  catch (Exception &e)
  {
    LOG4CXX_ERROR(logger_, "Got Log4CXX exception: " << e.what());
    rc = 1;
  }
  catch (exception &e)
  {
    LOG4CXX_ERROR(logger_, "Got exception:" << e.what());
    rc = 1;
  }
  catch (...)
  {
    LOG4CXX_ERROR(logger_, "Exception of unknown type!");
    rc = 1;
  }

  return rc;

}

void FrameReceiverApp::run(void)
{

  LOG4CXX_INFO(logger_,  "Running frame receiver");

  try {

    OdinData::IpcMessage config_msg, config_reply;
    config_.as_ipc_message(config_msg);

    controller_->configure(config_, config_msg, config_reply);

    controller_->run();

  }
  catch (OdinData::OdinDataException& e)
  {
    LOG4CXX_ERROR(logger_, "Frame receiver run failed: " << e.what());
  }
  catch (exception& e)
  {
    LOG4CXX_ERROR(logger_, "Generic exception during frame receiver run:\n" << e.what());
  }
  catch (...)
  {
    LOG4CXX_ERROR(logger_, "Unexpected exception during frame receiver run");
  }
}

void FrameReceiverApp::stop(void)
{
  controller_->stop();
}

//! Interrupt signal handler

void intHandler (int sig)
{
  FrameReceiver::FrameReceiverApp::stop ();
}

//! Main application entry point

int main (int argc, char** argv)
{
  int rc = 0;

  // Trap Ctrl-C and pass to interrupt handler
  signal (SIGINT, intHandler);
  signal (SIGTERM, intHandler);

  // Set the application path and locale for logging
  setlocale(LC_CTYPE, "UTF-8");
  OdinData::app_path = argv[0];

  // Create a FrameReceiverApp instance
  FrameReceiver::FrameReceiverApp fr_instance;

  // Parse command line arguments and set up node configuration
  rc = fr_instance.parse_arguments (argc, argv);

  if (rc == 0)
  {
    // Run the instance
    fr_instance.run ();
  }

  return rc;

}
