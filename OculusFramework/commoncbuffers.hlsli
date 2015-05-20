#include "commonstructs.hlsli"

cbuffer CameraBuffer : register(b0) {
    Camera camera;
};

cbuffer ObjectBuffer : register(b1) {
    Object object;
}
