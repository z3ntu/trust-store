/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <core/trust/daemon.h>

#include "process_exited_successfully.h"

#include <core/dbus/fixture.h>

#include <core/posix/exec.h>
#include <core/posix/fork.h>

#include <gmock/gmock.h>

#include <thread>

namespace
{
static constexpr const char* service_name
{
    "UnlikelyToEverExistOutsideOfTesting"
};

static constexpr const char* endpoint
{
    "/tmp/unlikely.to.ever.exist.outside.of.testing"
};

}

TEST(Daemon, unix_domain_agents_for_stub_and_skeleton_work_as_expected)
{
    // We fire up private bus instances to ensure tests working
    // during package builds.
    core::dbus::Fixture private_buses
    {
        core::dbus::Fixture::default_session_bus_config_file(),
        core::dbus::Fixture::default_system_bus_config_file()
    };

    std::remove(endpoint);

    // The stub accepting trust requests, relaying them via
    // the configured remote agent.
    core::posix::ChildProcess stub = core::posix::fork([]()
    {
        const char* argv[] =
        {
            __PRETTY_FUNCTION__,
            "--for-service", service_name,
            "--remote-agent", "UnixDomainSocketRemoteAgent",
            "--endpoint=/tmp/unlikely.to.ever.exist.outside.of.testing"
        };

        auto configuration = core::trust::Daemon::Stub::Configuration::from_command_line(6, argv);

        return core::trust::Daemon::Stub::main(configuration);
    }, core::posix::StandardStream::stdin | core::posix::StandardStream::stdout);

    // We really want to write EXPECT_TRUE(WaitForEndPointToBecomeAvailableFor(endpoint, 500ms));
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // The skeleton announcing itself to the stub instance started before via
    // the endpoint specified for the remote agent. In addition, it features a local
    // agent instance that always replies: denied.
    auto skeleton = core::posix::fork([]()
    {
        const char* argv[]
        {
            __PRETTY_FUNCTION__,
            "--remote-agent", "UnixDomainSocketRemoteAgent",
            "--endpoint=/tmp/unlikely.to.ever.exist.outside.of.testing",
            "--local-agent", "TheAlwaysDenyingLocalAgent",
            "--for-service", service_name,
            "--store-bus", "session_with_address_from_env"
        };

        auto configuration = core::trust::Daemon::Skeleton::Configuration::from_command_line(10, argv);

        return core::trust::Daemon::Skeleton::main(configuration);
    }, core::posix::StandardStream::empty);

    // Wait for everything to be setup
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // And inject a request into the stub
    std::string answer;

    for (int feature = 0; feature < 50; feature++)
    {
        stub.cin() << core::posix::this_process::instance().pid() << " " << ::getuid() << " " << feature << std::endl;
        stub.cout() >> answer;
    }

    // Wait for all requests to be answered
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Sigterm both stub and skeleton.
    skeleton.send_signal_or_throw(core::posix::Signal::sig_term);
    stub.send_signal_or_throw(core::posix::Signal::sig_term);

    // Expect both of them to exit with success.
    EXPECT_TRUE(ProcessExitedSuccessfully(skeleton.wait_for(core::posix::wait::Flags::untraced)));
    EXPECT_TRUE(ProcessExitedSuccessfully(stub.wait_for(core::posix::wait::Flags::untraced)));
}

TEST(Daemon, dbus_agents_for_stub_and_skeleton_work_as_expected)
{
    // We fire up private bus instances to ensure tests working
    // during package builds.
    core::dbus::Fixture private_buses
    {
        core::dbus::Fixture::default_session_bus_config_file(),
        core::dbus::Fixture::default_system_bus_config_file()
    };

    static std::string bus_arg{"--bus=session_with_address_from_env"};

    // The stub accepting trust requests, relaying them via
    // the configured remote agent.
    core::posix::ChildProcess stub = core::posix::fork([]()
    {
        const char* argv[] =
        {
            __PRETTY_FUNCTION__,
            "--for-service", service_name,
            "--remote-agent", "DBusRemoteAgent",
            bus_arg.c_str()
        };

        auto configuration = core::trust::Daemon::Stub::Configuration::from_command_line(6, argv);

        return core::trust::Daemon::Stub::main(configuration);
    }, core::posix::StandardStream::stdin | core::posix::StandardStream::stdout);

    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // The skeleton announcing itself to the stub instance started before via
    // the endpoint specified for the remote agent. In addition, it features a local
    // agent instance that always replies: denied.
    auto skeleton = core::posix::fork([]()
    {
        const char* argv[]
        {
            __PRETTY_FUNCTION__,
            "--remote-agent", "DBusRemoteAgent",
            bus_arg.c_str(),
            "--local-agent", "TheAlwaysDenyingLocalAgent",
            "--for-service", service_name,
            "--store-bus", "session_with_address_from_env"
        };

        auto configuration = core::trust::Daemon::Skeleton::Configuration::from_command_line(10, argv);

        return core::trust::Daemon::Skeleton::main(configuration);
    }, core::posix::StandardStream::empty);

    // Wait for everything to be setup
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // And inject a request into the stub
    std::string answer;

    for (int feature = 0; feature < 50; feature++)
    {
        stub.cin() << core::posix::this_process::instance().pid() << " " << ::getuid() << " " << feature << std::endl;
        stub.cout() >> answer;
    }

    // Wait for all requests to be answered
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Sigterm both stub and skeleton.
    skeleton.send_signal_or_throw(core::posix::Signal::sig_term);
    stub.send_signal_or_throw(core::posix::Signal::sig_term);

    // Expect both of them to exit with success.
    EXPECT_TRUE(ProcessExitedSuccessfully(skeleton.wait_for(core::posix::wait::Flags::untraced)));
    EXPECT_TRUE(ProcessExitedSuccessfully(stub.wait_for(core::posix::wait::Flags::untraced)));
}
