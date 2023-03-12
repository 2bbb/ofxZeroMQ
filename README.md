# ofxZeroMQ

[ofxZmq](https://github.com/satoruhiga/ofxZmq) alternavtive.

## Notice

* **this addon is still buggy phase!**
  * API will be change frequently.
  * if you find a bug, please post issue.
* tested on
  *  macOS 10.14.6 + Xcode11.2 + oF0.11.0 release
  *  **NOT** developed/tested on windows. maybe it can't be build.
     *  need to fill `libs/zeromq/src/platform.hpp` for windows env.

## Features

* basically, [satoruhiga/ofxZmq](https://github.com/satoruhiga/ofxZmq) compatible
* support zmq multipart message
* support auto reconnect
* static polymorphic get value / set value

## API

### ofxZeroMQMessage

### ofxZeroMQMultipartMessage

### ofxZeroMQSubscriber

### ofxZeroMQPublisher

## Dependencies

* [zeromq/libzmq v4.3.2](https://github.com/zeromq/libzmq/releases/tag/v4.3.2)
* [zeromq/cppzmq v4.6.0](https://github.com/zeromq/cppzmq/releases/tag/v4.6.0)

## Licenses for dependencies

### libzmq

* [GNU GPL v3](https://github.com/zeromq/libzmq/blob/v4.3.2/COPYING)

* [GNU LGPL v3](https://github.com/zeromq/libzmq/blob/v4.3.2/COPYING.LESSER)

* [RELICENSE](https://github.com/zeromq/libzmq/tree/v4.3.2/RELICENSE)

### cppzmq

* [MIT](https://github.com/zeromq/cppzmq/blob/v4.6.0/LICENSE)

## Update history

### 2020/01/28 ver 0.0.0_alpha

* initial buggy version

## License for this addon

* MIT

## Author

* ISHII 2bit [ISHII Tsuubito Program Office, HagukiWoKamuToRingoKaraTiGaDeru]
* i[at]2bit.jp

## At the last

Please create a new issue if there is a problem.

And please throw a pull request if you have a cool idea!!

If you get happy with using this addon, and you're rich, please donation for support continuous development.

Bitcoin: `17AbtW73aydfYH3epP8T3UDmmDCcXSGcaf`
