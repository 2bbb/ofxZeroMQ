#include "ofMain.h"
#include "ofxZeroMQ.h"

class ofApp : public ofBaseApp {
    ofxZeroMQSubscriber sub;
    ofImage image;
    int x;
    int y;
public:
    inline virtual void setup() override {
        sub.connect("ipc:///tmp/ofxZeroMQPubSubTest");
        
        ofSetWindowPosition(400, 0);
        ofSetWindowTitle("sub-example");
    }
    inline virtual void update() override {
        while(sub.hasWaitingMessage()) {
            ofxZeroMQMultipartMessage m;
            sub.receiveMultipart(m);
            std::string event = m[0];
            if(event == "mouse") {
                x = m[1];
                y = m[2];
            } else if(event == "data") {
                std::vector<std::uint8_t> data = m[1];
                std::ostringstream ss;
                
                ss << "data1:";
                for(auto i : data) ss << " " << +i;
                ofLogNotice() << ss.str();
                
                m[2].to(data);
                // or `data = m[2];`
                // or `data = m[2].get<std::vector<std::uint8_t>>();`
                ss = std::ostringstream{};
                ss << "data2:";
                for(auto i : data) ss << " " << +i;
                ofLogNotice() << ss.str();
            } else if(event == "grabber") {
                image = m[1];
                image.update();
            }
        }
    }
    inline virtual void draw() override {
        image.draw(0, 0);
        ofDrawCircle(x, y, 10);
    }
};

int main() {
    ofSetupOpenGL(400, 400, OF_WINDOW);
    ofRunApp(new ofApp());
}
