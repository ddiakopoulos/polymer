#version 330

const float pi = 3.14159265f;

uniform sampler2D s_blurTexure;

in vec2 v_texcoord;
out vec4 f_color;

// The sigma value for the gaussian function: higher value means more blur
// A good value for 13x13 is around 4 to 6
// A good value for 9x9 is around 3 to 5                         
// A good value for 7x7 is around 2.5 to 4                         
// A good value for 5x5 is around 2 to 3.5   
uniform float sigma;  

// This should usually be equal to                         
// 1.0f / texture_pixel_width for a horizontal blur, and                         
// 1.0f / texture_pixel_height for a vertical blur.
uniform float blurSize;

#if defined(VERTICAL_BLUR_13)
const float numBlurPixelsPerSide = 6.0f;
const vec2  blurMultiplyVec      = vec2(0.0f, 1.0f);
#elif defined(HORIZONTAL_BLUR_13)
const float numBlurPixelsPerSide = 6.0f;
const vec2  blurMultiplyVec      = vec2(1.0f, 0.0f);
#elif defined(VERTICAL_BLUR_9)
const float numBlurPixelsPerSide = 4.0f;
const vec2  blurMultiplyVec      = vec2(0.0f, 1.0f);
#elif defined(HORIZONTAL_BLUR_9)
const float numBlurPixelsPerSide = 4.0f;
const vec2  blurMultiplyVec      = vec2(1.0f, 0.0f);
#elif defined(VERTICAL_BLUR_7)
const float numBlurPixelsPerSide = 3.0f;
const vec2  blurMultiplyVec      = vec2(0.0f, 1.0f);
#elif defined(HORIZONTAL_BLUR_7)
const float numBlurPixelsPerSide = 3.0f;
const vec2  blurMultiplyVec      = vec2(1.0f, 0.0f);
#elif defined(VERTICAL_BLUR_5)
const float numBlurPixelsPerSide = 2.0f;
const vec2  blurMultiplyVec      = vec2(0.0f, 1.0f);
#elif defined(HORIZONTAL_BLUR_5)
const float numBlurPixelsPerSide = 2.0f;
const vec2  blurMultiplyVec      = vec2(1.0f, 0.0f);
#else
uniform float numBlurPixelsPerSide;
uniform vec2  blurMultiplyVec;
#endif

void main() 
{  
    // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)  
    vec3 incrementalGaussian;  
    incrementalGaussian.x = 1.0f / (sqrt(2.0f * pi) * sigma);  
    incrementalGaussian.y = exp(-0.5f / (sigma * sigma));  
    incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;  
    vec4 avgValue = vec4(0.0f, 0.0f, 0.0f, 0.0f);  

    // Central sample
    float coefficientSum = 0.0f; 
    avgValue += texture(s_blurTexure, v_texcoord.xy) * incrementalGaussian.x; 
    coefficientSum += incrementalGaussian.x;  
    incrementalGaussian.xy *= incrementalGaussian.yz;

    // Other samples in the window
    for (float i = 1.0f; i <= numBlurPixelsPerSide; i++) 
    {     
        avgValue += texture(s_blurTexure, v_texcoord.xy - i * blurSize * blurMultiplyVec) * incrementalGaussian.x;
        avgValue += texture(s_blurTexure, v_texcoord.xy + i * blurSize * blurMultiplyVec) * incrementalGaussian.x;
        coefficientSum += 2 * incrementalGaussian.x;    
        incrementalGaussian.xy *= incrementalGaussian.yz;  
    }  
    f_color = avgValue / coefficientSum;
    gl_FragDepth = f_color.r;
}