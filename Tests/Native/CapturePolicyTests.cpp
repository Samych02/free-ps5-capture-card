#include <chiaki/session.h>
#include <chiaki/launchspec.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <vector>
#include <iostream>
extern "C" {
#include <takion.pb.h>
#include <pb_decode.h>
ChiakiErrorCode capture_test_start_feedback(ChiakiStreamConnection *);
void capture_test_stop_feedback(ChiakiStreamConnection *);
ChiakiErrorCode capture_test_input_setup(ChiakiStreamConnection *);
ChiakiErrorCode capture_test_ack(ChiakiStreamConnection *);
ChiakiErrorCode capture_test_heartbeat(ChiakiStreamConnection *);
ChiakiErrorCode capture_test_launch(ChiakiStreamConnection *);
}
static int starts=0,stops=0,states=0;
static bool failStart=false,failSend=false;
static std::vector<int> messages;
static bool expectedCapture=false,formattedLaunch=false;
extern "C" int capture_test_launchspec_format(char *buf,size_t size,ChiakiLaunchSpec *spec) {
    assert(spec->capture_only==expectedCapture);
    assert(chiaki_launchspec_format(buf,size,spec)>0);
    assert(strstr(buf,expectedCapture?"\"connectedControllers\":[]":"\"connectedControllers\":[\"xinput\",\"ds3\",\"ds4\"]"));
    assert(strstr(buf,"\"width\":1920") && strstr(buf,"\"height\":1080"));
    formattedLaunch=true;return -1; // Stop real BIG path after inspecting its formatted launch; no ECDH/network setup.
}
extern "C" ChiakiErrorCode capture_test_feedback_init(ChiakiFeedbackSender *,ChiakiTakion *) {
    ++starts;return failStart?CHIAKI_ERR_THREAD:CHIAKI_ERR_SUCCESS;
}
extern "C" void capture_test_feedback_fini(ChiakiFeedbackSender *) {++stops;}
extern "C" ChiakiErrorCode capture_test_feedback_state(ChiakiFeedbackSender *,ChiakiControllerState *) {++states;return CHIAKI_ERR_SUCCESS;}
extern "C" ChiakiErrorCode capture_test_takion_send(ChiakiTakion *,uint8_t,uint16_t,uint8_t *data,size_t size,ChiakiSeqNum32 *) {
    tkproto_TakionMessage message{};auto stream=pb_istream_from_buffer(data,size);
    assert(pb_decode(&stream,tkproto_TakionMessage_fields,&message));
    messages.push_back(message.type);return failSend?CHIAKI_ERR_NETWORK:CHIAKI_ERR_SUCCESS;
}
struct Fixture {
    ChiakiLog log{};ChiakiSession session{};int sockets[2]{-1,-1};
    explicit Fixture(bool capture) {
        starts=stops=states=0;failStart=failSend=false;messages.clear();
        chiaki_log_init(&log,0,nullptr,nullptr);
        ChiakiConnectInfo info{};info.ps5=true;info.host="127.0.0.1";
        info.video_profile={1920,1080,60,15000,CHIAKI_CODEC_H265};
        if(capture)info.capture_only=true; // Legacy exercises the zero-initialized default.
        info.enable_keyboard=true;info.enable_dualsense=true;
        assert(chiaki_session_init(&session,&info,&log)==CHIAKI_ERR_SUCCESS);
        assert(session.connect_info.capture_only==capture);
        if(capture) {assert(!session.connect_info.enable_keyboard);assert(!session.connect_info.enable_dualsense);}
        else {session.connect_info.enable_keyboard=false;session.connect_info.enable_dualsense=false;}
        uint8_t synthetic[16]{};chiaki_rpcrypt_init_auth(&session.rpcrypt,CHIAKI_TARGET_PS5_1,synthetic,synthetic);
        assert(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)==0);session.ctrl.sock=sockets[0];
    }
    ~Fixture() {
        capture_test_stop_feedback(&session.stream_connection);
        session.ctrl.sock=CHIAKI_INVALID_SOCKET;
        for(auto fd:sockets)if(fd>=0)close(fd);
        chiaki_session_fini(&session);
    }
    std::vector<unsigned> controls() {
        std::vector<uint8_t> bytes;uint8_t chunk[256];ssize_t count;
        while((count=recv(sockets[1],chunk,sizeof(chunk),MSG_DONTWAIT))>0)bytes.insert(bytes.end(),chunk,chunk+count);
        std::vector<unsigned> types;
        for(size_t i=0;i<bytes.size();) {
            assert(bytes.size()-i>=8);
            uint32_t size=(uint32_t(bytes[i])<<24)|(uint32_t(bytes[i+1])<<16)|(uint32_t(bytes[i+2])<<8)|bytes[i+3];
            types.push_back(unsigned(bytes[i+4])*256+bytes[i+5]);i+=8+size;assert(i<=bytes.size());
        }
        return types;
    }
};
int main() {
    assert(chiaki_lib_init()==CHIAKI_ERR_SUCCESS);
    { Fixture f(true);auto &stream=f.session.stream_connection;
      expectedCapture=true;formattedLaunch=false;
      assert(capture_test_launch(&stream)==CHIAKI_ERR_UNKNOWN && formattedLaunch);
      assert(capture_test_start_feedback(&stream)==CHIAKI_ERR_SUCCESS);
      assert(!stream.feedback_sender_active && starts==0 && states==0);
      capture_test_stop_feedback(&stream);capture_test_stop_feedback(&stream);assert(stops==0);
      assert(capture_test_input_setup(&stream)==CHIAKI_ERR_SUCCESS);assert(messages.empty());
      ctrl_enable_features(&f.session.ctrl);assert(f.controls()==std::vector<unsigned>{0x910});
      assert(chiaki_session_connect_microphone(&f.session)!=CHIAKI_ERR_SUCCESS);
      assert(chiaki_session_toggle_microphone(&f.session,false)!=CHIAKI_ERR_SUCCESS);
      assert(chiaki_session_toggle_microphone(&f.session,true)!=CHIAKI_ERR_SUCCESS);
      ChiakiControllerState controller{};controller.buttons=1;
      auto original=f.session.controller_state;
      assert(chiaki_session_set_controller_state(&f.session,&controller)!=CHIAKI_ERR_SUCCESS);
      assert(memcmp(&original,&f.session.controller_state,sizeof(original))==0);
      assert(f.controls().empty());
      assert(capture_test_ack(&stream)==CHIAKI_ERR_SUCCESS);assert(capture_test_heartbeat(&stream)==CHIAKI_ERR_SUCCESS);
      assert((messages==std::vector<int>{tkproto_TakionMessage_PayloadType_STREAMINFOACK,tkproto_TakionMessage_PayloadType_HEARTBEAT}));
    }
    { Fixture f(false);auto &stream=f.session.stream_connection;
      expectedCapture=false;formattedLaunch=false;
      assert(capture_test_launch(&stream)==CHIAKI_ERR_UNKNOWN && formattedLaunch);
      assert(capture_test_start_feedback(&stream)==CHIAKI_ERR_SUCCESS);
      assert(stream.feedback_sender_active && starts==1 && states==1);
      assert(capture_test_start_feedback(&stream)==CHIAKI_ERR_SUCCESS && starts==1 && states==1);
      capture_test_stop_feedback(&stream);capture_test_stop_feedback(&stream);assert(stops==1);
      ChiakiControllerState controller{};controller.buttons=1;
      assert(chiaki_session_set_controller_state(&f.session,&controller)==CHIAKI_ERR_SUCCESS);
      assert(f.session.controller_state.buttons==1);
      assert(capture_test_input_setup(&stream)==CHIAKI_ERR_SUCCESS);
      assert((messages==std::vector<int>{tkproto_TakionMessage_PayloadType_CONTROLLERCONNECTION,tkproto_TakionMessage_PayloadType_STREAMINFO}));
      ctrl_enable_features(&f.session.ctrl);assert((f.controls()==std::vector<unsigned>{0x36,0x36,0x910}));
      assert(chiaki_session_connect_microphone(&f.session)==CHIAKI_ERR_SUCCESS);
      assert(chiaki_session_toggle_microphone(&f.session,false)==CHIAKI_ERR_SUCCESS);
      assert((f.controls()==std::vector<unsigned>{0x30,0x36}));
    }
    { Fixture f(false);failStart=true;
      assert(capture_test_start_feedback(&f.session.stream_connection)==CHIAKI_ERR_THREAD);
      capture_test_stop_feedback(&f.session.stream_connection);assert(starts==1 && stops==0 && states==0);
      failSend=true;assert(capture_test_input_setup(&f.session.stream_connection)==CHIAKI_ERR_NETWORK);
      assert(messages.size()==1); // A failed controller announcement never proceeds to microphone setup.
    }
    std::cout<<"PASS: real control/protobuf output, capture-only and legacy, explicit input rejection, feedback lifecycle/failure, media ACK/heartbeat\n";
}
