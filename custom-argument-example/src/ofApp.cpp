#include "ofMain.h"
#include "ofxZeroMQ.h"

// if class is in your namespace,
// then implement to_zmq_message & from_zmq_message to same namespace
namespace your_namespace {
    struct custom_object {
        int p;
        int q;
    };
    
    void to_zmq_message(ofxZeroMQMessage &m, const custom_object &obj) {
        m.set(obj.p, obj.q);
    }

    void from_zmq_message(const ofxZeroMQMessage &m, custom_object &obj) {
        std::memcpy(&obj.p, m.data(), sizeof(int));
        std::memcpy(&obj.q, (char *)m.data() + sizeof(int), sizeof(int));
    }
};

// or, if class is in not your namespace,
namespace third_party_namespace {
    struct some_object {
        int p;
        int q;
    };
};

// then implement specialized adl_converter
namespace ofxZeroMQ {
    template <>
    struct adl_converter<third_party_namespace::some_object> {
        static void to_zmq_message(ofxZeroMQMessage &m, const third_party_namespace::some_object &obj) {
            m.set(obj.p, obj.q);
        }

        static void from_zmq_message(const ofxZeroMQMessage &m, third_party_namespace::some_object &obj) {
            std::memcpy(&obj.p, m.data(), sizeof(int));
            std::memcpy(&obj.q, (char *)m.data() + sizeof(int), sizeof(int));
        }
    };
};

class ofApp : public ofBaseApp {
    ofxZeroMQSubscriber sub;
    ofxZeroMQPublisher pub;
public:
    inline virtual void setup() override {
        sub.connect("tcp://localhost:26666");
        pub.bind("tcp://*:26666");
    }
    inline virtual void update() override {
        if(sub.hasWaitingMessage()) {
            ofxZeroMQMultipartMessage m;
            sub.receive(m);
            std::string type = m[0];
            if(type == "your") {
                your_namespace::custom_object obj;
                obj = m[1];
                ofLogNotice() << obj.p << " " << obj.q;
            } else if(type == "third") {
                third_party_namespace::some_object obj;
                obj = m[1];
                ofLogNotice() << obj.p << " " << obj.q;
            }
        }
    }
    inline virtual void draw() override {
    }
    inline virtual void keyPressed(int key) override {
        if(key == ' ') {
            your_namespace::custom_object obj;
            obj.p = 1;
            obj.q = 2;
            pub.sendMultipart("your", obj);
        } else if(key == OF_KEY_RETURN) {
            third_party_namespace::some_object obj;
            obj.p = 1;
            obj.q = 2;
            pub.sendMultipart("third", obj);
        }
    }
};

int main() {
    ofSetupOpenGL(400, 400, OF_WINDOW);
    ofRunApp(new ofApp());
}

