/*
 *    Copyright (C) 2013 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mongo/shell/shell_options.h"

#include <boost/filesystem/operations.hpp>

#include "mongo/base/status.h"
#include "mongo/bson/util/builder.h"
#include "mongo/db/server_options.h"
#include "mongo/shell/shell_utils.h"
#include "mongo/util/net/sock.h"
#include "mongo/util/net/ssl_options.h"
#include "mongo/util/options_parser/environment.h"
#include "mongo/util/options_parser/option_description.h"
#include "mongo/util/options_parser/option_section.h"
#include "mongo/util/options_parser/options_parser.h"
#include "mongo/util/options_parser/startup_option_init.h"
#include "mongo/util/version.h"

namespace mongo {

    moe::OptionSection mongoShellOptions("options");
    moe::Environment mongoShellParsedOptions;
    ShellGlobalParams shellGlobalParams;

    Status addMongoShellOptions(moe::OptionSection* options) {

        typedef moe::OptionDescription OD;
        typedef moe::PositionalOptionDescription POD;

        Status ret = options->addOption(OD("shell", "shell", moe::Switch,
                    "run the shell after executing files", true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("nodb", "nodb", moe::Switch,
                    "don't connect to mongod on startup - no 'db address' arg expected", true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("norc", "norc", moe::Switch,
                    "will not run the \".mongorc.js\" file on start up", true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("quiet", "quiet", moe::Switch, "be less chatty", true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("port", "port", moe::String, "port to connect to" , true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("host", "host", moe::String, "server to connect to" , true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("eval", "eval", moe::String, "evaluate javascript" , true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("username", "username,u", moe::String,
                    "username for authentication" , true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("password", "password,p", moe::String,
                    "password for authentication" , true, moe::Value(),
                    moe::Value(std::string(""))));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("authenticationDatabase", "authenticationDatabase", moe::String,
                    "user source (defaults to dbname)" , true, moe::Value(std::string(""))));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("authenticationMechanism", "authenticationMechanism",
                    moe::String, "authentication mechanism", true,
                    moe::Value(std::string("MONGODB-CR"))));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("help", "help,h", moe::Switch, "show this usage information",
                    true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("version", "version", moe::Switch, "show version information",
                    true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("verbose", "verbose", moe::Switch, "increase verbosity", true));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("ipv6", "ipv6", moe::Switch,
                    "enable IPv6 support (disabled by default)", true));
        if (!ret.isOK()) {
            return ret;
        }

#ifdef MONGO_SSL
        ret = addSSLClientOptions(options);
        if (!ret.isOK()) {
            return ret;
        }
#endif

        ret = options->addOption(OD("dbaddress", "dbaddress", moe::String, "dbaddress" , false));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addOption(OD("files", "files", moe::StringVector, "files" , false));
        if (!ret.isOK()) {
            return ret;
        }
        // for testing, kill op will also be disabled automatically if the tests starts a mongo
        // program
        ret = options->addOption(OD("nokillop", "nokillop", moe::Switch, "nokillop", false));
        if (!ret.isOK()) {
            return ret;
        }
        // for testing, will kill op without prompting
        ret = options->addOption(OD("autokillop", "autokillop", moe::Switch, "autokillop", false));
        if (!ret.isOK()) {
            return ret;
        }

        ret = options->addPositionalOption(POD("dbaddress", moe::String, 1));
        if (!ret.isOK()) {
            return ret;
        }
        ret = options->addPositionalOption(POD("files", moe::String, -1));
        if (!ret.isOK()) {
            return ret;
        }

        return Status::OK();
    }

    std::string getMongoShellHelp(const StringData& name, const moe::OptionSection& options) {
        StringBuilder sb;
        sb << "MongoDB shell version: " << mongo::versionString << "\n";
        sb << "usage: " << name << " [options] [db address] [file names (ending in .js)]\n"
        << "db address can be:\n"
        << "  foo                   foo database on local machine\n"
        << "  192.169.0.5/foo       foo database on 192.168.0.5 machine\n"
        << "  192.169.0.5:9999/foo  foo database on 192.168.0.5 machine on port 9999\n"
        << options.helpString() << "\n"
        << "file names: a list of files to run. files have to end in .js and will exit after "
        << "unless --shell is specified";
        return sb.str();
    }

    Status handlePreValidationMongoShellOptions(const moe::Environment& params,
                                                const std::vector<std::string>& args) {
        if ( mongoShellParsedOptions.count( "help" ) ) {
            std::cout << getMongoShellHelp(args[0], mongoShellOptions) << std::endl;
            ::_exit(EXIT_CLEAN);
        }
        if ( mongoShellParsedOptions.count( "version" ) ) {
            cout << "MongoDB shell version: " << mongo::versionString << endl;
            ::_exit(EXIT_CLEAN);
        }
        if (mongoShellParsedOptions.count("quiet")) {
            mongo::serverGlobalParams.quiet = true;
        }
#ifdef MONGO_SSL
        Status ret = storeSSLClientOptions(mongoShellParsedOptions);
        if (!ret.isOK()) {
            return ret;
        }
#endif
        if (mongoShellParsedOptions.count("ipv6")) {
            mongo::enableIPv6();
        }
        if (mongoShellParsedOptions.count("verbose")) {
            logger::globalLogDomain()->setMinimumLoggedSeverity(logger::LogSeverity::Debug(1));
        }

        return Status::OK();
    }

    Status storeMongoShellOptions(const moe::Environment& params,
                                  const std::vector<std::string>& args) {
        if (mongoShellParsedOptions.count("port")) {
            shellGlobalParams.port = mongoShellParsedOptions["port"].as<string>();
        }

        if (mongoShellParsedOptions.count("host")) {
            shellGlobalParams.dbhost = mongoShellParsedOptions["host"].as<string>();
        }

        if (mongoShellParsedOptions.count("eval")) {
            shellGlobalParams.script = mongoShellParsedOptions["eval"].as<string>();
        }

        if (mongoShellParsedOptions.count("username")) {
            shellGlobalParams.username = mongoShellParsedOptions["username"].as<string>();
        }

        if (mongoShellParsedOptions.count("password")) {
            shellGlobalParams.usingPassword = true;
            shellGlobalParams.password = mongoShellParsedOptions["password"].as<string>();
        }

        if (mongoShellParsedOptions.count("authenticationDatabase")) {
            shellGlobalParams.authenticationDatabase =
                mongoShellParsedOptions["authenticationDatabase"].as<string>();
        }

        if (mongoShellParsedOptions.count("authenticationMechanism")) {
            shellGlobalParams.authenticationMechanism =
                mongoShellParsedOptions["authenticationMechanism"].as<string>();
        }

        if ( mongoShellParsedOptions.count( "shell" ) ) {
            shellGlobalParams.runShell = true;
        }
        if ( mongoShellParsedOptions.count( "nodb" ) ) {
            shellGlobalParams.nodb = true;
        }
        if ( mongoShellParsedOptions.count( "norc" ) ) {
            shellGlobalParams.norc = true;
        }
        if ( mongoShellParsedOptions.count( "files" ) ) {
            shellGlobalParams.files = mongoShellParsedOptions["files"].as< vector<string> >();
        }
        if ( mongoShellParsedOptions.count( "nokillop" ) ) {
            mongo::shell_utils::_nokillop = true;
        }
        if ( mongoShellParsedOptions.count( "autokillop" ) ) {
            shellGlobalParams.autoKillOp = true;
        }

        /* This is a bit confusing, here are the rules:
         *
         * if nodb is set then all positional parameters are files
         * otherwise the first positional parameter might be a dbaddress, but
         * only if one of these conditions is met:
         *   - it contains no '.' after the last appearance of '\' or '/'
         *   - it doesn't end in '.js' and it doesn't specify a path to an existing file */
        if ( mongoShellParsedOptions.count( "dbaddress" ) ) {
            string dbaddress = mongoShellParsedOptions["dbaddress"].as<string>();
            if (shellGlobalParams.nodb) {
                shellGlobalParams.files.insert( shellGlobalParams.files.begin(), dbaddress );
            }
            else {
                string basename = dbaddress.substr( dbaddress.find_last_of( "/\\" ) + 1 );
                if (basename.find_first_of( '.' ) == string::npos ||
                        (basename.find(".js", basename.size() - 3) == string::npos &&
                         !::mongo::shell_utils::fileExists(dbaddress))) {
                    shellGlobalParams.url = dbaddress;
                }
                else {
                    shellGlobalParams.files.insert( shellGlobalParams.files.begin(), dbaddress );
                }
            }
        }

        if ( shellGlobalParams.url == "*" ) {
            std::cerr << "ERROR: " << "\"*\" is an invalid db address" << std::endl;
            std::cerr << getMongoShellHelp(args[0], mongoShellOptions) << std::endl;
            ::_exit(EXIT_BADOPTIONS);
        }

        return Status::OK();
    }

    MONGO_GENERAL_STARTUP_OPTIONS_REGISTER(MongoShellOptions)(InitializerContext* context) {
        return addMongoShellOptions(&mongoShellOptions);
    }

    MONGO_STARTUP_OPTIONS_PARSE(MongoShellOptions)(InitializerContext* context) {
        moe::OptionsParser parser;
        Status ret = parser.run(mongoShellOptions, context->args(), context->env(),
                                &mongoShellParsedOptions);
        if (!ret.isOK()) {
            std::cerr << ret.reason() << std::endl;
            std::cerr << "try '" << context->args()[0]
                      << " --help' for more information" << std::endl;
            ::_exit(EXIT_BADOPTIONS);
        }
        return Status::OK();
    }

    MONGO_STARTUP_OPTIONS_VALIDATE(MongoShellOptions)(InitializerContext* context) {
        Status ret = handlePreValidationMongoShellOptions(mongoShellParsedOptions, context->args());
        if (!ret.isOK()) {
            return ret;
        }
        ret = mongoShellParsedOptions.validate();
        if (!ret.isOK()) {
            return ret;
        }
        return Status::OK();
    }

    MONGO_STARTUP_OPTIONS_STORE(MongoShellOptions)(InitializerContext* context) {
        return storeMongoShellOptions(mongoShellParsedOptions, context->args());
    }
}
