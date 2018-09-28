// Copyright 2017 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <ostream>
#include <sstream>
#include <thread>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

#include "mongo-seastar.hpp"
#include <core/reactor.hh>
#include <core/app-template.hh>
#include <core/thread.hh>

using bsoncxx::builder::basic::kvp;

int main (int argc, char *argv[]) {
    app_template app;
    app.run_deprecated(argc, argv, [] {
        return seastar::async([] {
            // The mongocxx::instance constructor and destructor initialize and shut down the driver,
            // respectively. Therefore, a mongocxx::instance must be created before using the driver and
            // must remain alive for as long as the driver is in use.
            mongocxx::instance inst{};
            mongocxx::uri uri{"mongodb://localhost:27017/?minPoolSize=3&maxPoolSize=3"};

            mongocxx::pool pool{uri};

            std::vector<std::string> collection_names = {"foo", "bar", "baz"};

            // Each client and collection can only be used in a single thread.
            auto client = pool.acquire();
            auto coll = (*client)["test"]["foo"];
            coll.delete_many({});

            bsoncxx::types::b_int64 index = {1};
            coll.insert_one(bsoncxx::builder::basic::make_document(kvp("x", index)));

            if (auto doc = (*client)["test"]["foo"].find_one({})) {
                // In order to ensure that the newline is printed immediately after the document,
                // they need to be streamed to std::cout as a single string.
                std::stringstream ss;
                ss << bsoncxx::to_json(*doc) << std::endl;

                std::cout << ss.str();
            }
        });
    });

    return 0;
}