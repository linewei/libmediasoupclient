#include "Exception.hpp"
#include "Handler.hpp"
#include "catch.hpp"
#include "data/parameters.hpp"
#include <future>
#include <memory>

static const json TransportRemoteParameters = generateTransportRemoteParameters();
static const json IceServers                = json::array();
static const json ProprietaryConstraints    = json::array();
static const json RtpParametersByKind       = generateRtpParametersByKind();
static const std::string IceTransportPolicy("all");

class FakeHandlerListener : public Handler::Listener
{
public:
	std::future<void> OnConnect(json& /*transportLocalParameters*/) override
	{
		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(
	  webrtc::PeerConnectionInterface::IceConnectionState /*connectionState*/) override{};
};

TEST_CASE("Handler", "[Handler]")
{
	SECTION("'Handler::GetNativeRtpCapabilities()' succeeds")
	{
		json rtpCapabilities;

		REQUIRE_NOTHROW(rtpCapabilities = Handler::GetNativeRtpCapabilities());

		REQUIRE(rtpCapabilities["codecs"].is_array());
		REQUIRE(rtpCapabilities["fecMechanisms"].is_array());
		REQUIRE(rtpCapabilities["headerExtensions"].is_array());
	}

	SECTION("'Handler::GetName()' succeeds")
	{
		REQUIRE_NOTHROW(Handler::GetName());
	}
}

TEST_CASE("SendHandler", "[Handler][SendHandler]")
{
	static rtc::scoped_refptr<webrtc::AudioSourceInterface> source;
	static rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

	static FakeHandlerListener handlerListener;

	static SendHandler sendHandler(
	  &handlerListener,
	  TransportRemoteParameters,
	  IceServers,
	  IceTransportPolicy,
	  ProprietaryConstraints,
	  RtpParametersByKind);

	static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

	SECTION("'sendHandler.Send()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(nullptr, false).get(), Exception);
	}

	SECTION("'sendHandler.Send()' succeeds if a track is provided")
	{
		auto source = pc->CreateAudioSource();
		track       = pc->CreateAudioTrack("test-track-id", source);

		json rtpParameters;

		REQUIRE_NOTHROW(rtpParameters = sendHandler.Send(track, false).get());
		REQUIRE(rtpParameters["codecs"].size() == 1);
		REQUIRE(rtpParameters["headerExtensions"].size() == 3);
	}

	SECTION("'sendHandler.Send()' fails if track is already handled")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(track, false).get(), Exception);
	}

	SECTION("'sendHandler.ReplaceTrack()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.ReplaceTrack(nullptr, nullptr).get(), Exception);
	}

	SECTION("'sendHandler.ReplaceTrack()' succeeds if a new track is provided")
	{
		auto newTrack = pc->CreateAudioTrack("test-new-track-id", source);

		REQUIRE_NOTHROW(sendHandler.ReplaceTrack(track, newTrack).get());

		track = newTrack;
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.SetMaxSpatialLayer(nullptr, "").get(), Exception);
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.SetMaxSpatialLayer(track, "").get());
	}

	SECTION("'sendHandler.GetSenderStats()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.GetSenderStats(nullptr).get(), Exception);
	}

	SECTION("'sendHandler.GetSenderStats()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.GetSenderStats(track).get());
	}

	SECTION("'sendHandler.StopSending()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(nullptr).get(), Exception);
	}

	SECTION("'sendHandler.StopSending()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.StopSending(track).get());
	}

	SECTION("'sendHandler.StopSending()' fails if track is not being sent")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(track).get(), Exception);
	}

	SECTION("'sendHandler.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendHandler.RestartIce(remoteIceParameters).get());
	}

	SECTION("'sendHandler.UpdateIceServers()' succeeds")
	{
		REQUIRE_NOTHROW(sendHandler.UpdateIceServers(IceServers).get());
	}
}

TEST_CASE("RecvHandler", "[Handler][RecvHandler]")
{
	/* clang-format off */
	static json rtpParameters =
	{
		{ "encodings",
			{
				{
					{ "ssrc", 11111111 }
				}
			}
		},
		{ "rtcp",
			{
				{ "cname",       "test-cname" },
				{ "reducedSize", true         },
				{ "mux",         true         }
			}
		}
	};
	/* clang-format on */

	static FakeHandlerListener handlerListener;

	static RecvHandler recvHandler(
	  &handlerListener,
	  TransportRemoteParameters,
	  IceServers,
	  IceTransportPolicy,
	  ProprietaryConstraints,
	  RtpParametersByKind);

	SECTION("'recvHander.Receive()' succeeds if correct rtpParameters are provided")
	{
		REQUIRE_NOTHROW(recvHandler.Receive("test", "audio", rtpParameters).get());
	}

	SECTION("'recvHandler.Receive()' fails if rtpParameters are already handled")
	{
		REQUIRE_THROWS_AS(recvHandler.Receive("test", "audio", rtpParameters).get(), Exception);
	}

	SECTION("'recvHandler.GetReceiverStats()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.GetReceiverStats("unknown").get(), Exception);
	}

	SECTION("'recvHandler.GetReceiverStats()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.GetReceiverStats("test").get());
	}

	SECTION("'recvHandler.StopReceiving()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.StopReceiving("unknown").get(), Exception);
	}

	SECTION("'recvHandler.StopReceiving()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.StopReceiving("test").get());
	}

	SECTION("'recvHandler.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvHandler.RestartIce(remoteIceParameters).get());
	}

	SECTION("'recvHandler.UpdateIceServers()' succeeds")
	{
		REQUIRE_NOTHROW(recvHandler.UpdateIceServers(IceServers).get());
	}
}