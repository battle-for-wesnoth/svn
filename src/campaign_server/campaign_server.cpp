#include "../config.hpp"
#include "../network.hpp"
#include "../publish_campaign.hpp"
#include "../util.hpp"

#include "SDL.h"

#include <iostream>

namespace {

config construct_message(const std::string& msg)
{
	config cfg;
	cfg.add_child("message")["message"] = msg;
	return cfg;
}

config construct_error(const std::string& msg)
{
	config cfg;
	cfg.add_child("error")["message"] = msg;
	return cfg;
}

class campaign_server
{
public:
	explicit campaign_server(const std::string& cfgfile);
	void run();
private:

	const config& campaigns() const { return *cfg_.child("campaigns"); }
	config& campaigns() { return *cfg_.child("campaigns"); }
	config cfg_;
	const std::string file_;
	const network::manager net_manager_;
	const network::server_manager server_manager_;

};

campaign_server::campaign_server(const std::string& cfgfile)
     : cfg_(read_file(cfgfile)), file_(cfgfile),
       server_manager_(lexical_cast_default<int>(cfg_["port"],15002))
{
	if(cfg_.child("campaigns") == NULL) {
		cfg_.add_child("campaigns");
	}
}

void campaign_server::run()
{
	for(int increment = 0; ; ++increment) {
		try {
			//write config to disk every ten minutes
			if((increment%(60*10*2)) == 0) {
				write_file(file_,cfg_.write());
			}

			network::process_send_queue();

			network::connection sock = network::accept_connection();
			if(sock) {
				std::cerr << "received connection from " << network::ip_address(sock) << "\n";
			}

			config data;
			while((sock = network::receive_data(data)) != NULL) {
				if(data.child("request_campaign_list") != NULL) {
					config campaign_list = campaigns();
					config::child_list cmps = campaign_list.get_children("campaign");
					for(config::child_list::iterator i = cmps.begin(); i != cmps.end(); ++i) {
						(**i)["passphrase"] = "";
					}

					config response;
					response.add_child("campaigns",campaign_list);
					network::send_data(response,sock);
				} else if(const config* req = data.child("request_campaign")) {
					config* const campaign = campaigns().find_child("campaign","name",(*req)["name"]);
					if(campaign == NULL) {
						network::send_data(construct_error("Campaign not found."),sock);
					} else {
						config cfg;
						compression_schema schema;
						cfg.read_compressed(read_file((*campaign)["filename"]),schema);
						network::queue_data(cfg,sock);

						const int downloads = lexical_cast_default<int>((*campaign)["downloads"],0)+1;
						(*campaign)["downloads"] = lexical_cast<std::string>(downloads);
					}

				} else if(data.child("request_terms") != NULL) {
					network::send_data(construct_message("All campaigns uploaded to this server must be licensed under the terms of the GNU General Public License (GPL). By uploading content to this server, you certify that you have the right to place the content under the conditions of the GPL, and choose to do so."),sock);
				} else if(const config* upload = data.child("upload")) {
					config* campaign = campaigns().find_child("campaign","name",(*upload)["name"]);
					if(campaign != NULL && (*campaign)["passphrase"] != (*upload)["passphrase"]) {
						network::send_data(construct_error("The campaign already exists, and your passphrase was incorrect."),sock);
					} else if(campaign_name_legal((*upload)["name"]) == false) {
						network::send_data(construct_error("The name of the campaign is invalid"),sock);
					} else {
						if(campaign == NULL) {
							campaign = &campaigns().add_child("campaign");
						}

						(*campaign)["name"] = (*upload)["name"];
						(*campaign)["filename"] = (*upload)["name"];
						(*campaign)["passphrase"] = (*upload)["passphrase"];
						(*campaign)["author"] = (*upload)["author"];
						(*campaign)["description"] = (*upload)["description"];
						(*campaign)["version"] = (*upload)["version"];
						(*campaign)["icon"] = (*upload)["icon"];

						if((*campaign)["downloads"].empty()) {
							(*campaign)["downloads"] = "0";
						}

						const config* const data = upload->child("data");
						if(data != NULL) {
							compression_schema schema;
							const std::string& filedata = data->write_compressed(schema);
							write_file((*campaign)["filename"],filedata);
							(*campaign)["size"] = lexical_cast<std::string>(filedata.size());
						}

						write_file(file_,cfg_.write());
						network::send_data(construct_message("Campaign accepted."),sock);
					}
				} else if(const config* erase = data.child("delete")) {
					config* const campaign = campaigns().find_child("campaign","name",(*erase)["name"]);
					if(campaign == NULL) {
						network::send_data(construct_error("The campaign does not exist."),sock);
						continue;
					}

					if((*campaign)["passphrase"] != (*erase)["passphrase"]) {
						network::send_data(construct_error("The passphrase is incorrect."),sock);
						continue;
					}

					//erase the campaign
					write_file((*campaign)["filename"],"");
					
					const config::child_list& campaigns_list = campaigns().get_children("campaign");
					const size_t index = std::find(campaigns_list.begin(),campaigns_list.end(),campaign) - campaigns_list.begin();
					campaigns().remove_child("campaign",index);
					write_file(file_,cfg_.write());
					network::send_data(construct_message("Campaign erased."),sock);
				}
			}
		} catch(network::error& e) {
			if(!e.socket) {
				std::cerr << "fatal network error\n";
				break;
			} else {
				e.disconnect();
			}
		} catch(config::error& e) {
			std::cerr << "error in receiving data...\n";
		}

		SDL_Delay(500);
	}
}

}

int main(int argc, char** argv)
{
	try {
		campaign_server("server.cfg").run();
	} catch(config::error& e) {
		std::cerr << "Could not parse config file\n";
	} catch(io_exception& e) {
		std::cerr << "File I/O error\n";
	}
}
