#ifndef SERVER_LOG_H
#define SERVER_LOG_H

#include <iostream>

// ANSI 颜色码（Linux 终端原生支持）
#define SLOG_RESET   "\033[0m"
#define SLOG_RED     "\033[31m"
#define SLOG_GREEN   "\033[32m"
#define SLOG_YELLOW  "\033[33m"
#define SLOG_CYAN    "\033[36m"

#define LOG_INFO(msg)  std::cout << SLOG_GREEN << "[信息] " << SLOG_RESET << msg << std::endl
#define LOG_WARN(msg)  std::cout << SLOG_YELLOW << "[警告] " << SLOG_RESET << msg << std::endl
#define LOG_ERR(msg)   std::cerr << SLOG_RED << "[错误] " << SLOG_RESET << msg << std::endl

#endif // SERVER_LOG_H
