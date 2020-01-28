//
//  ofxZeroMQConvertFunctions.h
//
//  Created by 2bit on 2020/01/26.
//

#ifndef ofxZeroMQConvertFunctions_h
#define ofxZeroMQConvertFunctions_h

#include <type_traits>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "ofConstants.h"
#include "ofGraphics.h"
#include "ofPixels.h"
#include "ofImage.h"
#include "ofVectorMath.h"

/* standard layout types:
 * ofColor_<T>
 * ofVecNf
 * glm::vecN
 * ofMatrix3x3/4x4
 * glm::matN
 * ofQuaternion
 * glm::quat
 */

namespace ofxZeroMQ {
    namespace detail {
#pragma mark ofBuffer
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofBuffer &data)
        { m.rebuild(data.getData(), data.size()); };

        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofBuffer &data)
        {
            data.resize(m.size());
            data.set((const char *)m.data(), m.size());
        }

#pragma mark ofRectangle
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofRectangle &rect) {
            m.set(rect.position.x, rect.position.y, rect.position.z, rect.width, rect.height);
        }

        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofRectangle &rect)
        {
            m.setTo(rect.position.x, rect.position.y, rect.position.z, rect.width, rect.height);
        }

#pragma mark ofPixels
        template <typename pix_type>
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofPixels_<pix_type> &pix)
        {
            std::uint32_t size[2];
            size[0] = pix.getWidth(),
            size[1] = pix.getHeight();
            ofPixelFormat format = pix.getPixelFormat();
            
            m.rebuild(sizeof(uint32_t) * 2 + sizeof(ofPixelFormat) + sizeof(pix_type) * pix.size());
            auto offset = m.set(size, format);
            m.copyFrom(pix.getData(), sizeof(pix_type) * pix.size(), offset);
        }

        template <typename pix_type>
        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofPixels_<pix_type> &pix)
        {
            std::uint32_t size[2];
            ofPixelFormat pixel_format;
            auto offset = m.setTo(size, pixel_format);
            pix.setFromPixels((pix_type *)((char *)m.data() + offset),
                              size[0],
                              size[1],
                              pixel_format);
        }

#pragma mark ofBaseHasPixels
        template <typename pix_type>
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofBaseHasPixels_<pix_type> &pix)
        { to_zmq_message(m, pix.getPixels()); };

        template <typename pix_type>
        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofBaseHasPixels_<pix_type> &pix)
        { from_zmq_message(m, pix.getPixels()); };
    }; // detail
}; // ofxZeroMQ

#endif /* ofxZeroMQConvertFunctions_h */
