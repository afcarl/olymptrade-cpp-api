/*
* olymptrade-cpp-api - Olymptrade C++ API client
*
* Copyright (c) 2018 Elektro Yar. Email: electroyar2@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef OLYMPTRADEAPI_HPP_INCLUDED
#define OLYMPTRADEAPI_HPP_INCLUDED
//------------------------------------------------------------------------------
#include <server_wss.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <chrono>
#include <iostream>
//------------------------------------------------------------------------------
//#define OLYMPTRADE_API_USE_LOG
//------------------------------------------------------------------------------
class OlympTradeAPI
{
public:
        using json = nlohmann::json;
        using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

        enum ErrorType {
                OK = 0,
                NO_AUTHORIZATION = -1,
                NO_COMMAND = -2,
                UNKNOWN_ERROR = -3,
                NO_INIT = -4,
                NO_CONNECTION = -5,
        };

        enum ContractType {
                BUY = 1,
                SELL = -1,
        };

        // длительности контракта
        enum DurationType {
                TICKS = 0,
                SECONDS = 1,
                MINUTES = 2,
                HOURS = 3,
                DAYS = 4,
        };

        struct OlympTradeSymbolParam {
                double min_amount = 0.0;
                double max_amount = 0.0;
                double winperc = 0.0;             /**< Процент выплат */
                int min_duration = 0;
                int max_duration = 0;
                bool locked = false;            /**< Валютная пара заблокирована? */
                std::string name;

                void update(json &j) {
                        if(j["min_amount"] != nullptr)
                                min_amount = j["min_amount"];
                        if(j["max_amount"] != nullptr)
                                max_amount = j["max_amount"];
                        if(j["winperc"] != nullptr)
                                winperc = j["winperc"];
                                winperc /= 100.0;
                        if(j["min_duration"] != nullptr)
                                min_duration = j["min_duration"];
                        if(j["max_duration"] != nullptr)
                                max_duration = j["max_duration"];
                        if(j["locked"] != nullptr)
                                locked = j["locked"];
                        if(j["name"] != nullptr)
                                name = j["name"];
                        if(locked) {
                                winperc = 0.0;
                        }
                };

                OlympTradeSymbolParam(json &j) {
                        update(j);
                };

                OlympTradeSymbolParam() {

                };
        };
private:
        WsServer server_;
        std::shared_ptr<WsServer::Connection> echo_socket_;
        std::shared_ptr<WsServer::Connection> echo_control_;
        std::queue<std::string> send_queue_echo_socket_; // Очередь сообщений
        std::queue<std::string> send_queue_echo_control_; // Очередь сообщений
        std::mutex queue_echo_socket_mutex_;
        std::mutex queue_echo_control_mutex_;
        std::atomic<bool> is_echo_socket_;
        std::atomic<bool> is_echo_control_;
        std::mutex echo_socket_mutex_;
        std::mutex echo_control_mutex_;

        // поток выплат и котировок
        std::vector<std::string> symbols_;
        std::unordered_map<std::string, int> map_symbol_;
        std::mutex map_symbol_mutex_;
        std::vector<std::vector<double>> open_data_;
        std::vector<std::vector<double>> high_data_;
        std::vector<std::vector<double>> low_data_;
        std::vector<std::vector<double>> close_data_;
        std::vector<std::vector<unsigned long long>> time_data_;
        std::mutex quotation_mutex_;
        std::atomic<unsigned long long> server_time_;
        std::atomic<bool> is_get_server_time_;
        std::atomic<double> balance_real_;
        std::atomic<double> balance_demo_;
        std::atomic<bool> is_get_balance_demo_;
        std::atomic<bool> is_get_balance_real_;

        //
        std::vector<double> history_open_;
        std::vector<double> history_high_;
        std::vector<double> history_low_;
        std::vector<double> history_close_;
        std::vector<unsigned long long> history_time_;
        std::atomic<bool> is_recive_history_;
        std::atomic<bool> is_send_cmd_history_;
        std::mutex history_mutex_;
        //
        std::unordered_map<std::string, int> map_proposal_;
        std::vector<OlympTradeSymbolParam> proposal_data_;
        std::atomic<bool> is_get_proposal_data_;
        std::mutex proposal_mutex_;

//------------------------------------------------------------------------------
        int send_echo_socket(std::string message)
        {
                if(!is_echo_socket_)
                        return NO_CONNECTION;
#               ifdef OLYMPTRADE_API_USE_LOG
                std::cout << "Server: Message send: \"" << message << "\" from " << echo_socket_.get() << std::endl;
#               endif

        }
//------------------------------------------------------------------------------
        void check_demo_balance_message(json &j, json::iterator &it_data)
        {
                double temp = (*it_data)[0]["value"];
                balance_demo_ = temp;
                is_get_balance_demo_ = true;
        }
//------------------------------------------------------------------------------
        void check_time_message(json &j, json::iterator &it_data)
        {
                unsigned long long epoch = (*it_data)[0]["timestamp"];
                unsigned long long last_server_time = server_time_;
                server_time_ = std::max(epoch, last_server_time);
                is_get_server_time_ = true;
        }
//------------------------------------------------------------------------------
        void check_proposal_message(json &j, json::iterator &it_data)
        {
                const int array_size = (*it_data).size();
                bool is_get_data = false;
                for(int i = 0; i < array_size; ++i) {
                        if((*it_data)[i] == nullptr)
                                continue;
                        auto it_name = (*it_data)[i].find("name");
                        auto it_pair = (*it_data)[i].find("pair");

                        std::string symbol = "";
                        if(it_name != (*it_data)[i].end())
                                symbol = (*it_name);
                        if(it_pair != (*it_data)[i].end())
                                symbol = (*it_pair);

                        if(symbol.size() > 0) {
                                proposal_mutex_.lock();
                                if(map_proposal_.find(symbol) == map_proposal_.end()) {
                                        proposal_data_.push_back(OlympTradeSymbolParam((*it_data)[i]));
                                        map_proposal_[symbol] = proposal_data_.size() - 1;
                                        is_get_data = true;
                                } else {
                                        int indx = map_proposal_[symbol];
                                        proposal_data_[indx].update((*it_data)[i]);
                                }
                                proposal_mutex_.unlock();
                        } // if
                }
                if(is_get_data)
                        is_get_proposal_data_ = true;
        }
//------------------------------------------------------------------------------
        void check_tick_message(json &j, json::iterator &it_data)
        {
                const int number_symbols = (*it_data).size(); // количество валютных пар
                for(int i = 0; i < number_symbols; ++i) {
                        if((*it_data)[i]["q"] != nullptr) {
                                const std::string symbol = (*it_data)[i]["p"];
                                map_symbol_mutex_.lock();
                                const auto it_map_symbol_ = map_symbol_.find(symbol);
                                if(it_map_symbol_ != map_symbol_.end()) {
                                        const int indx = it_map_symbol_->second;
                                        map_symbol_mutex_.unlock();
                                        quotation_mutex_.lock();
                                        if(time_data_[indx].size() > 0) { // если данные уже есть
                                                const unsigned long long last_epoch = time_data_[indx].back();
                                                quotation_mutex_.unlock();
                                                const unsigned long long epoch = (*it_data)[i]["t"];
                                                double quotation = (*it_data)[i]["q"];

                                                const unsigned long long last_server_time = server_time_;
                                                server_time_ = std::max(epoch, last_server_time);
                                                is_get_server_time_ = true;

                                                const unsigned long long open_time = (epoch / 60) * 60;
                                                if(open_time > last_epoch) { // если это новая свеча
                                                        quotation_mutex_.lock();
                                                        time_data_[indx].push_back(open_time);
                                                        close_data_[indx].push_back(quotation);
                                                        open_data_[indx].push_back(quotation);
                                                        low_data_[indx].push_back(quotation);
                                                        high_data_[indx].push_back(quotation);
                                                        quotation_mutex_.unlock();
                                                } else { // если та же самая свеча
                                                        quotation_mutex_.lock();
                                                        const int last_indx = time_data_[indx].size() - 1;
                                                        time_data_[indx][last_indx] = open_time;
                                                        close_data_[indx][last_indx] = quotation;
                                                        if(quotation < low_data_[indx][last_indx])
                                                                low_data_[indx][last_indx] = quotation;
                                                        if(quotation > high_data_[indx][last_indx])
                                                                high_data_[indx][last_indx] = quotation;
                                                        quotation_mutex_.unlock();
                                                }
                                        } else { // если вектор был пустой
                                                quotation_mutex_.unlock();
                                                const unsigned long long epoch = (*it_data)[i]["t"];
                                                double quotation = (*it_data)[i]["q"];
                                                const unsigned long long last_server_time = server_time_;
                                                server_time_ = std::max(epoch, last_server_time);
                                                is_get_server_time_ = true;
                                                const unsigned long long open_time = (epoch / 60) * 60;

                                                quotation_mutex_.lock();
                                                time_data_[indx].push_back(open_time);
                                                close_data_[indx].push_back(quotation);
                                                open_data_[indx].push_back(quotation);
                                                low_data_[indx].push_back(quotation);
                                                high_data_[indx].push_back(quotation);
                                                quotation_mutex_.unlock();
                                        }
                                } else {
                                        map_symbol_mutex_.unlock();
                                }
                        }
                } // for i
        }
//------------------------------------------------------------------------------
        void parse_json(std::string &str)
        {
                try {
                        json j = json::parse(str);
                        for(int i = 0; i < j.size(); ++i) {
                                auto it_data = j[i].find("d");
                                if(it_data != j[i].end()) {
                                        int indx = j[i]["e"];
                                        switch(indx) {
                                        case 1: // получили тик
                                                check_tick_message(j, it_data);
                                                break;
                                        case 2: // получаем свечу
                                                break;
                                        case 4: // получаем данные котировок
                                                break;
                                        case 52: // получен баланс

                                                break;
                                        case 70: // получаем проценты выплат
                                        case 71:
                                        case 72:
                                                check_proposal_message(j, it_data);
                                                break;
                                        case 132: // получено время
                                                check_time_message(j, it_data);
                                                break;
                                        }
                                }
                        }
                }
                catch(...) {
                        std::cout << "OlympTradeAPI: on_message error! Message: " <<
                                str << std::endl;
                }
        }
//------------------------------------------------------------------------------
        void parse_control_json(std::string &str)
        {
                try {
                        json j = json::parse(str);
                        if(j["data"] != nullptr) {
                                if(j["data"].is_array()) {
                                        const int array_size = j["data"].size();
                                        //std::cout << "array_size " << j_array_size << std::endl;
                                        history_mutex_.lock();
                                        history_open_.resize(array_size);
                                        history_high_.resize(array_size);
                                        history_low_.resize(array_size);
                                        history_close_.resize(array_size);
                                        history_time_.resize(array_size);
                                        for(int i = 0; i < array_size; ++i) {
                                                history_open_[i] = j["data"][i]["open"];
                                                history_high_[i] = j["data"][i]["high"];
                                                history_low_[i] = j["data"][i]["low"];
                                                history_close_[i] = j["data"][i]["close"];
                                                history_time_[i] = j["data"][i]["time"];
                                        }
                                        is_recive_history_ = true;
                                        history_mutex_.unlock();
                                }
                        }

                        if(j["user"]["server_time"] != nullptr) {
                                server_time_ = j["server_time"];
                                is_get_server_time_ = true;
                        }

                        if(j["user"]["balance"] != nullptr) {
                                balance_real_ = j["user"]["balance"];
                                is_get_balance_real_ = true;
                        }

                        if(j["user"]["balance_demo"] != nullptr) {
                                balance_demo_ = j["user"]["balance_demo"];
                                is_get_balance_demo_ = true;
                        }

                        if(j["echo_control"] == 1) {
                                std::cout << "echo_control = 1" << std::endl;
                        }
                }
                catch (json::parse_error& e) {
                        // output exception information
                        std::cout << "message: " << e.what() << '\n'
                          << "exception id: " << e.id << '\n'
                          << "byte position of error: " << e.byte << std::endl;
                }
                catch(...) {
                        std::cout << "OlympTradeAPI: on_message error! Message: " <<
                                str << std::endl;
                }
        }
//------------------------------------------------------------------------------
public:
//------------------------------------------------------------------------------
        OlympTradeAPI() : is_echo_socket_(false), is_echo_control_(false),
                is_get_balance_demo_(false), is_get_balance_real_(false),
                is_get_server_time_(false), is_get_proposal_data_(false),
                server_time_(0), balance_real_(0), balance_demo_(0)
        {
                server_.config.port = 8080;
                auto &echo_socket = server_.endpoint["^/echo_socket/?$"];
                echo_socket.on_message = [&](std::shared_ptr<WsServer::Connection> connection,
                        std::shared_ptr<WsServer::InMessage> in_message) {
                        auto out_message = in_message->string();
#                       ifdef OLYMPTRADE_API_USE_LOG
                        std::cout << "Server (echo_socket): Message received: \"" <<
                                out_message << "\" from " << connection.get() << std::endl;
#                       endif
                        parse_json(out_message);
                };
                echo_socket.on_open = [&](std::shared_ptr<WsServer::Connection> connection) {
                        std::cout << "Server (echo_socket): Opened connection " << connection.get() << std::endl;
                        echo_socket_mutex_.lock();
                        echo_socket_ = connection;
                        echo_socket_mutex_.unlock();
                        is_echo_socket_ = true;
                };
                // See RFC 6455 7.4.1. for status codes
                echo_socket.on_close = [&](std::shared_ptr<WsServer::Connection> connection,
                        int status,
                        const std::string & /*reason*/) {
                        std::cout << "Server (echo_socket): Closed connection " << connection.get() << " with status code " << status << std::endl;
                        is_echo_socket_ = false;
                        is_get_proposal_data_ = false;
                };
                // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                echo_socket.on_error = [&](std::shared_ptr<WsServer::Connection> connection,
                        const SimpleWeb::error_code &ec) {
                        std::cout << "Server (echo_socket): Error in connection " << connection.get() << ". "
                        << "Error: " << ec << ", error message: " << ec.message() << std::endl;
                        is_echo_socket_ = false;
                        is_get_proposal_data_ = false;
                };

                auto &echo_control = server_.endpoint["^/echo_control/?$"];

                echo_control.on_message = [&](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message) {
                        auto out_message = in_message->string();
#                       ifdef OLYMPTRADE_API_USE_LOG
                        std::cout << "Server (echo_control): Message received: \"" << out_message << "\" from " << connection.get() << std::endl;
#                       endif
                        parse_control_json(out_message);
                };

                echo_control.on_open = [&](std::shared_ptr<WsServer::Connection> connection) {
                        std::cout << "Server (echo_control): Opened connection " << connection.get() << std::endl;
                        // запоминаем соединение
                        echo_control_mutex_.lock();
                        echo_control_ = connection;
                        echo_control_mutex_.unlock();
                        is_echo_control_ = true;
                        // отправляем команду на перезагрузку процентов выплат
                        json j;
                        j["cmd"] = "reload_winperc";
                        std::string message = j.dump();
                        // отправляем сообщение про переинициализацию
                        echo_control_mutex_.lock();
                        connection->send(message, [&](const SimpleWeb::error_code &ec) {
                                if(ec) {
                                        std::cout << "Server: Error sending message. " <<
                                        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                                        "Error: " << ec << ", error message: " << ec.message() << std::endl;
                                }
                        });
                        echo_control_mutex_.unlock();
                };

                // See RFC 6455 7.4.1. for status codes
                echo_control.on_close = [&](std::shared_ptr<WsServer::Connection> connection, int status, const std::string & /*reason*/) {
                        std::cout << "Server (echo_control): Closed connection " << connection.get() << " with status code " << status << std::endl;
                        is_echo_control_ = false;
                };

                // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                echo_control.on_error = [&](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
                        std::cout << "Server (echo_control): Error in connection " << connection.get() << ". "
                        << "Error: " << ec << ", error message: " << ec.message() << std::endl;
                        is_echo_control_ = false;
                };

                std::thread server_thread([&]() {
                        while(true) {
                                server_.start();
                                is_echo_socket_ = false;
                                is_echo_control_ = false;
                                std::this_thread::sleep_for(std::chrono::seconds(5));
                        }
                });

                server_thread.detach();

                while(!is_onnection());
        };
//------------------------------------------------------------------------------
        /** \brief Состояние соединения
         * \return вернет true если соединение установлено
         */
        inline bool is_onnection()
        {
                if(is_echo_socket_ && is_echo_control_)
                        return true;
                return false;
        }
//------------------------------------------------------------------------------
        inline int get_proposal_data(std::vector<OlympTradeSymbolParam> &data)
        {
                if(!is_echo_socket_ || !is_echo_control_)
                        return NO_CONNECTION;
                if(!is_get_proposal_data_)
                        return UNKNOWN_ERROR;
                proposal_mutex_.lock();
                data = proposal_data_;
                proposal_mutex_.unlock();
                return OK;
        }
//------------------------------------------------------------------------------
        /** \brief Получить время сервера
         * \param timestamp Время сервера
         * \return состояние ошибки (0 в случае успеха, иначе см. ErrorType)
         */
        inline int get_servertime(unsigned long long &timestamp)
        {
                if(is_get_server_time_) {
                        timestamp = server_time_;
                        is_get_server_time_ = false;
                        return OK;
                } else {
                        return NO_INIT;
                }
        }
//------------------------------------------------------------------------------
};
#endif // OLYMPTRADEAPI_HPP_INCLUDED
