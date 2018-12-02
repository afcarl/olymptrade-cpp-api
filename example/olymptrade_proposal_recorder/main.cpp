#include <iostream>
#include "OlympTradeAPI.hpp"
#include "xtime.hpp"
#include <fstream>
#include <dir.h>
#include <stdlib.h>

#define BUILD_VER 1.0

using json = nlohmann::json;

void write_binary_file(std::string file_name,
                       std::vector<OlympTradeAPI::OlympTradeSymbolParam> &symbols_proposal,
                       unsigned long long timestamp);
void make_commit(std::string disk, std::string path, std::string new_file);

int main() {
        OlympTradeAPI iOlympTradeAPI;
        std::cout << "start!" << std::endl;
        json j_settings;
        std::ifstream i("settings.json");
        i >> j_settings;
        i.close();
        std::cout << std::setw(4) << j_settings << std::endl;
        const std::string folder_name = j_settings["folder"];
        const std::string disk_name = j_settings["disk"];
        const std::string path = j_settings["path"];
        int is_use_git = j_settings["git"];
        std::string old_file_name = "";

        // сохраним список валютных пар и параметры в отдельный файл
        std::vector<OlympTradeAPI::OlympTradeSymbolParam> _symbols_proposal;
        while(iOlympTradeAPI.get_proposal_data(_symbols_proposal) != OlympTradeAPI::OK);

        json j_pp;
        for(size_t i = 0; i < _symbols_proposal.size(); ++i) {
                j_pp["symbols"][i] = _symbols_proposal[i].name;
        }

        std::string folder_path = disk_name + ":\\" + path + "\\" + folder_name;
        std::string file_name_pp = folder_path + "\\parameters.json";
        mkdir(folder_path.c_str());
        std::ofstream fp(file_name_pp);
        fp << std::setw(4) << j_pp << std::endl;
        fp.close();
        make_commit(disk_name, path, file_name_pp);
        //

        std::cout << "..." << std::endl;


        unsigned long long server_time = xtime::get_unix_timestamp();
        unsigned long long last_server_time = xtime::get_unix_timestamp();
        while(true) {
                unsigned long long server_time = xtime::get_unix_timestamp();
                if(last_server_time < server_time) {
                        last_server_time = server_time;
                } else {
                        std::this_thread::yield();
                        continue;
                }
                std::vector<OlympTradeAPI::OlympTradeSymbolParam> symbols_proposal;
                if(iOlympTradeAPI.get_proposal_data(symbols_proposal) == OlympTradeAPI::OK) {
                        for(size_t i = 0; i < symbols_proposal.size(); ++i) {
                                std::cout << symbols_proposal[i].name << " " << symbols_proposal[i].winperc << std::endl;
                        }
                        // сохраняем список
                        mkdir(folder_path.c_str());
                        xtime::DateTime iTime(server_time);
                        std::string file_chunk_name = "proposal_" +
                                                      std::to_string(iTime.day) + "_" +
                                                      std::to_string(iTime.month) + "_" +
                                                      std::to_string(iTime.year);

                        const std::string file_name = folder_path + "\\" +
                                                      file_chunk_name +
                                                      ".hex";

                        write_binary_file(file_name, symbols_proposal, server_time);
                        if(old_file_name != file_name && is_use_git) {
                                std::thread make_thread([=]() {
                                        if(old_file_name == "") {
                                                make_commit(disk_name, path, file_name);
                                        } else {
                                                make_commit(disk_name, path, old_file_name);
                                        }
                                });
                                make_thread.detach();
                                old_file_name = file_name;
                        }
                }
                std::cout << "time: " << server_time << std::endl;
        }
}

void write_binary_file(std::string file_name,
                       std::vector<OlympTradeAPI::OlympTradeSymbolParam> &symbols_proposal,
                       unsigned long long timestamp)
{
        // проверяем, был ли создан файл?
        std::ifstream fin(file_name);
        if(!fin) {
                // сохраняем заголовок файла
                std::ofstream fout(file_name);
                json j;
                for(size_t i = 0; i < symbols_proposal.size(); ++i) {
                        j["symbols"][i] = symbols_proposal[i].name;
                }
                const int _sample_len = sizeof(unsigned char) * symbols_proposal.size() + sizeof (unsigned long long);
                j["sample_len"] = _sample_len;
                fout << j.dump() << "\n";
                fout.close();
                fin.close();
        } else {
                fin.close();
        }
        // сохраняем
        std::ofstream fout(file_name, std::ios_base::binary | std::ios::app);
        for(size_t i = 0; i < symbols_proposal.size(); ++i) {
                unsigned char temp_prop = symbols_proposal[i].winperc * 100;
                fout.write(reinterpret_cast<char *>(&temp_prop),sizeof (temp_prop));
        }
        fout.write(reinterpret_cast<char *>(&timestamp),sizeof (timestamp));
        fout.close();
}

void make_commit(std::string disk, std::string path, std::string new_file)
{
        std::string str_return_hd = "cd " + disk + ":\\";
        std::cout << str_return_hd << std::endl;

        std::string str_cd_path = "cd " + path;
        std::cout << str_cd_path << std::endl;

        std::string str_git_add = "git add " + new_file;
        std::cout << str_git_add << std::endl;

        std::string str_git_commit = "git commit -a -m \"update\"";
        std::cout << str_git_commit << std::endl;

        std::string str_git_pull = "git pull";
        std::cout << str_git_pull << std::endl;

        std::string str_git_push = "git push";
        std::cout << str_git_push << std::endl;

        std::string msg = str_return_hd + " && " + str_cd_path + " && " + str_git_add + " && " + str_git_commit + " && " + str_git_pull + " & " + str_git_push;
        system(msg.c_str());
}
