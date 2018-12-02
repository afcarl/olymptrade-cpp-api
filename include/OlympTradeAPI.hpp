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
#define OLYMPTRADE_API_USE_LOG
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
        };

        enum ContractType {
                BUY = 1,
                SELL = -1,
        };
private:
        WsServer server_;
        std::shared_ptr<WsServer::Connection> echo_socket_;
        std::shared_ptr<WsServer::Connection> echo_control_;
        std::atomic<bool> is_echo_socket_;
        std::atomic<bool> is_echo_control_;

        // поток выплат и котировок
        std::vector<std::string> symbols_;
        std::unordered_map<std::string, int> map_symbol_;
        std::vector<std::vector<double>> open_data_;
        std::vector<std::vector<double>> high_data_;
        std::vector<std::vector<double>> low_data_;
        std::vector<std::vector<double>> close_data_;
        std::vector<std::vector<unsigned long long>> time_data_;
        std::mutex map_symbol_mutex_;
//------------------------------------------------------------------------------
        void check_tick_message(json &j, json::iterator &it_data)
        {
                int number_symbols = (*it_data).size(); // количество валютных пар
                for(int i = 0; i < number_symbols; ++i) {
                        if(j[0]["d"][i]["q"] != nullptr) {
                                std::string symbol = j[0]["d"][i]["p"];
                                map_symbol_mutex_.lock();
                                auto it_map_symbol_ = map_symbol_.find(symbol);
                                if(it_map_symbol_ != map_symbol_.end()) {
                                        int indx = it_map_symbol_->second;
                                        if(time_data_[indx].size() > 0) { // если данные уже есть
                                                unsigned long long last_epoch = time_data_[indx].back();
                                                unsigned long long epoch = j[0]["d"][i]["t"];
                                                double quotation = j[0]["d"][i]["q"];
                                        }
                                }
                                map_symbol_mutex_.unlock();
                        }
                }
        }
//------------------------------------------------------------------------------
        void parse_json(std::string &str)
        {
                try {
                        json j = json::parse(str);
                        auto it_data = j[0].find("d");
                        if(it_data != j[0].end()) {
                                int indx = j[0]["e"];
                                switch(indx) {
                                case 1: // получили тик

                                        break;
                                case 1:
                                        break;
                                }
                        }
                }
                catch(...) {
                        std::cout << "OlympTradeAPI: on_message error! Message: " <<
                                str << std::endl;
                }
        }
//------------------------------------------------------------------------------
public:
//------------------------------------------------------------------------------
        OlympTradeAPI()
        {
                server_.config.port = 8080;
                auto &echo_socket = server.endpoint["^/echo_socket/?$"];
                echo_socket.on_message = [&](std::shared_ptr<WsServer::Connection> connection,
                        std::shared_ptr<WsServer::InMessage> in_message) {
                        auto out_message = in_message->string();
#                       ifdef OLYMPTRADE_API_USE_LOG
                        std::cout << "Server (echo_socket): Message received: \"" <<
                                out_message << "\" from " << connection.get() << std::endl;
#                       endif
                        parse_json(out_message);
                }
        }
//------------------------------------------------------------------------------
        inline bool is_onnection()
        {
                if(is_echo_socket_ && is_echo_control_)
                        return true;
                return false;
        }

};
#endif // OLYMPTRADEAPI_HPP_INCLUDED
