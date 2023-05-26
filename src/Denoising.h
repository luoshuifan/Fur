#pragma once

#include <mitsuba/render/sampler.h>

void* __restrict allocAligned(size_t size) {
#if defined(__WINDOWS__)
    return _aligned_malloc(size, L1_CACHE_LINE_SIZE);
#elif defined(__OSX__)
    /* OSX malloc already returns 16-byte aligned data suitable
       for AltiVec and SSE computations */
    return malloc(size);
#else
    return memalign(L1_CACHE_LINE_SIZE, size);
#endif
}

void freeAligned(void* ptr) {
#if defined(__WINDOWS__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

inline float Gaussian(float x, float sigma)
{
    return std::exp(-(x * x) / (2 * sigma * sigma));
}

inline float Luminance(float r, float g, float b)
{
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

inline int32_t clamp(int32_t x, int32_t a, int32_t b)
{
    if (x < a)
        x = a;
    if (x > b)
        x = b;
    return x;
}

inline void normalize(float& a, float& b, float& c, bool bClamp = false)
{
    if (std::isinf(a) || std::isnan(a) ||
        std::isinf(a) || std::isnan(a) ||
        std::isinf(a) || std::isnan(a))
        std::cout << "normalize in denoising.h number " << a << " " << b << " " << c << std::endl;

    float len = std::sqrt(a * a + b * b + c * c);
    if (len != 0)
    {
        a = a / len;
        b = b / len;
        c = c / len;
        if (bClamp)
        {
            a = clamp(a, 0, 1);
            b = clamp(b, 0, 1);
            c = clamp(c, 0, 1);
        }
    }
}


float KernelWeight[]{ 3.0f / 8,1.0f / 4,1.0f / 16 };

inline float Weight(int32_t x, int32_t y, int32_t i, int32_t j, 
    int32_t size_x, int32_t size_y, int32_t channel, 
    float* color, float* normal,float* pos,
    float& sumValue_r, float& sumValue_g, float& sumValue_b,int32_t weightIndex,
    bool bWithAll = false)
{
    uint32_t index = 0;
    if (weightIndex == 1 || weightIndex == 2)
        weightIndex = 1;
    else if (weightIndex == 3 || weightIndex == 4)
        weightIndex = 2;

    i = clamp(i, 0, size_x - 1);
    j = clamp(j, 0, size_y - 1);
    int32_t offset_xy = (x + size_x * y) * channel;
    int32_t offset_ij = (i + size_x * j) * channel;

    if (i < 0 || i > size_x - 1 ||
        j < 0 || j > size_y - 1)
        std::cout << "weight " << i << " " << j << std::endl;

    float r_xy = *(color + offset_xy), g_xy = *(color + offset_xy + 1), b_xy = *(color + offset_xy + 2);
    float r_ij = *(color + offset_ij), g_ij = *(color + offset_ij + 1), b_ij = *(color + offset_ij + 2);

    float lum_xy = Luminance(r_xy, g_xy, b_xy);
    float lum_ij = Luminance(r_ij, g_ij, b_ij);

    float n0_xy = *(normal + offset_xy), n1_xy = *(normal + offset_xy + 1), n2_xy = *(normal + offset_xy + 2);
    float n0_ij = *(normal + offset_ij), n1_ij = *(normal + offset_ij + 1), n2_ij = *(normal + offset_ij + 2);

    float n0 = n0_xy - n0_ij, n1 = n1_xy - n1_ij, n2 = n2_xy - n2_ij;
    float cosvalue = n0 * n0 + n1 * n1 + n2 * n2;

    //if(cosvalue != 0)
    //    std::cout << "cos " << cosvalue << " " << n0_xy << " " << n1_xy << " " <<  n2_xy
    //    << " " << n0_ij << " " << n1_ij << " " << n2_ij << std::endl;

    float posx_xy = *(pos + offset_xy), posy_xy = *(pos + offset_xy + 1), posz_xy = *(pos + offset_xy + 2);
    float posx_ij = *(pos + offset_ij), posy_ij = *(pos + offset_ij + 1), posz_ij = *(pos + offset_ij + 2);

    float posx = posx_xy - posx_ij, posy = posy_xy - posy_ij, posz = posz_xy - posz_ij;
    float dotPos = posx * posx + posy * posy + posz * posz;

    float sigma = 0.797884560803;
    float weight = Gaussian(std::fabs(lum_xy - lum_ij), 0.6);
    //weight = 1;
    if (bWithAll)
    {
        //weight *= Gaussian(std::fabs(cosvalue), 0.3) *
        //    Gaussian(std::fabs(dotPos), 0.3);
        weight *= Gaussian(std::fabs(cosvalue), 0.255) *
            Gaussian(std::fabs(dotPos), 0.255);
    }

    sumValue_r += weight * r_ij;
    sumValue_g += weight * g_ij;
    sumValue_b += weight * b_ij;

    /*if (lenxy != 0)
    {
        sumValue_r = n0_xy * 0.5 + 0.5;
        sumValue_g = n1_xy * 0.5 + 0.5;
        sumValue_b = n2_xy * 0.5 + 0.5;
    }
    else
    {
        sumValue_r = n0_xy ;
        sumValue_g = n1_xy ;
        sumValue_b = n2_xy;
    }*/

    return weight;

}



constexpr int32_t kernel = 2;
constexpr int32_t IterNum = 2;
constexpr int32_t ShowSpp = 0;

void FilterImage(int32_t size_x, int32_t size_y, int32_t channel, float* source, float* dest,
    float* normal, float* positions, bool bWithAll = false, float scale = 1)
{
    for (int32_t it = 0; it <= IterNum; ++it)
    {
        int32_t step = std::pow(2, it);

        for (int32_t x = 0; x < size_x; ++x)
        {
            for (int32_t y = 0; y < size_y; ++y)
            {
                size_t offset = ((size_t)x + size_x * (size_t)y) * channel;

                float sumWeight = 0, sumValue_r = 0, sumValue_g = 0, sumValue_b = 0;
                float* value = (dest + offset);


                for (int32_t i = -kernel * (step); i <= kernel * (step); i += (step))
                {
                    for (int32_t j = -kernel * (step); j <= kernel * (step); j += (step))
                    {
                        float weight = Weight(x, y, x + i, y + j, size_x, size_y, channel, source, normal, positions,
                            sumValue_r, sumValue_g, sumValue_b, std::abs(i / step) + std::abs(j / step), bWithAll);
                        sumWeight += weight;
                    }
                }

                value[0] = sumValue_r / sumWeight * scale;
                value[1] = sumValue_g / sumWeight * scale;
                value[2] = sumValue_b / sumWeight * scale;

                if (sumWeight <= 0.001)
                {
                    value[0] = value[1] = value[2] = 0;
                }
            }
        }

        /*for (int32_t x = 0; x < size_x; ++x)
        {
            for (int32_t y = 0; y < size_y; ++y)
            {
                size_t offset = ((size_t)x + size_x * (size_t)y) * channel;
                float sumWeight = 0, sumValue_r = 0, sumValue_g = 0, sumValue_b = 0;
                float* value = (color + offset);

                for (int32_t i = -kernel * step; i <= kernel * step; i += step)
                {
                    float weight = Weight(x, y, x, y + i, size_x, size_y, channel, color, normal, pos,
                        sumValue_r, sumValue_g, sumValue_b, std::abs(i/step));
                    sumWeight += weight;

                }

                value[0] = sumValue_r / sumWeight;
                value[1] = sumValue_g / sumWeight;
                value[2] = sumValue_b / sumWeight;
            }
        }*/
    }
}

void copyImage(int32_t size_x, int32_t size_y, int32_t channel, float* source, float* dest,float Scale = 1)
{
    for(int32_t i = 0; i < size_x; ++i)
        for (int32_t j = 0; j < size_y; ++j)
        {
            int32_t offset = (j * size_x + i) * channel;
            *(dest + offset) = (*(source + offset)) * Scale;
            *(dest + offset + 1) = (*(source + offset + 1)) * Scale;
            *(dest + offset + 2) = (*(source + offset + 2)) * Scale;

        }
}

void BlendPixel(int32_t blend0, int32_t blend1, int32_t dest, float* color)
{
    float r = color[blend0 + 0], g = color[blend0 + 1], b = color[blend0 + 2];
    float r1 = color[blend1 + 0], g1 = color[blend1 + 1], b1 = color[blend1 + 2];
    color[dest] = (r + r1) / 2.0f;
    color[dest + 1] = (g + g1) / 2.0f;
    color[dest + 2] = (b + b1) / 2.0f;
}

void BlendPixel(int32_t blend0, int32_t blend1, int32_t blend2, int32_t dest, float* color)
{
    float r = color[blend0 + 0], g = color[blend0 + 1], b = color[blend0 + 2];
    float r1 = color[blend1 + 0], g1 = color[blend1 + 1], b1 = color[blend1 + 2];
    float r2 = color[blend2 + 0], g2 = color[blend2 + 1], b2 = color[blend2 + 2];

    color[dest] = (r + r1 + r2) / 3.0f;
    color[dest + 1] = (g + g1 + g2) / 3.0f;
    color[dest + 2] = (b + b1 + b2) / 3.0f;
}

void BlendPixel(int32_t blend0, int32_t blend1, int32_t blend2, int32_t blend3, int32_t dest, float* color)
{
    float r = color[blend0 + 0], g = color[blend0 + 1], b = color[blend0 + 2];
    float r1 = color[blend1 + 0], g1 = color[blend1 + 1], b1 = color[blend1 + 2];
    float r2 = color[blend2 + 0], g2 = color[blend2 + 1], b2 = color[blend2 + 2];
    float r3 = color[blend3 + 0], g3 = color[blend3 + 1], b3 = color[blend3 + 2];

    color[dest] = (r + r1 + r2 + r3) / 4.0f;
    color[dest + 1] = (g + g1 + g2 + g3) / 4.0f;
    color[dest + 2] = (b + b1 + b2 + b3) / 4.0f;
}

void GetSelection(int32_t OffSet, int32_t SPPNum, int32_t MISPP,
    int32_t size_x, int32_t size_y, int32_t channel, int32_t pos_x, int32_t pos_y,
    uint8_t* LayerData, float* Selections)
{
    uint8_t* SelLayerData = LayerData;
    //float* layer = (float*)(SelLayerData + OffSet * 1);
    //Selections[0] = layer[LayerOffset];
    //Selections[1] = layer[LayerOffset + 1];
    //Selections[2] = layer[LayerOffset + 2];

    for (int32_t SPPIndex = 0; SPPIndex < SPPNum; ++SPPIndex)
    {
        float* layer = (float*)(SelLayerData + OffSet * SPPIndex);

        int32_t LayerOffset = (pos_x + pos_y * size_x) * channel;
        Selections[SPPIndex * 3 + 3 * 9 * 2] = layer[LayerOffset];
        Selections[SPPIndex * 3 + 1 + 3 * 9 * 2] = layer[LayerOffset + 1];
        Selections[SPPIndex * 3 + 2 + 3 * 9 * 2] = layer[LayerOffset + 2];
    }

    //15 4=11
    BlendPixel(3 * (9 * 2 + 0), 3 * (9 * 2 + 1), 3 * (9 * 2 + SPPNum + 0), Selections);
    BlendPixel(3 * (9 * 2 + 0), 3 * (9 * 2 + 2), 3 * (9 * 2 + SPPNum + 1), Selections);
    BlendPixel(3 * (9 * 2 + 0), 3 * (9 * 2 + 3), 3 * (9 * 2 + SPPNum + 2), Selections);
    BlendPixel(3 * (9 * 2 + 1), 3 * (9 * 2 + 2), 3 * (9 * 2 + SPPNum + 3), Selections);
    BlendPixel(3 * (9 * 2 + 1), 3 * (9 * 2 + 3), 3 * (9 * 2 + SPPNum + 4), Selections);
    BlendPixel(3 * (9 * 2 + 2), 3 * (9 * 2 + 3), 3 * (9 * 2 + SPPNum + 5), Selections);

    BlendPixel(3 * (9 * 2 + 0), 3 * (9 * 2 + 1), 3 * (9 * 2 + 2), 3 * (9 * 2 + SPPNum + 6), Selections);
    BlendPixel(3 * (9 * 2 + 0), 3 * (9 * 2 + 2), 3 * (9 * 2 + 3), 3 * (9 * 2 + SPPNum + 7), Selections);
    BlendPixel(3 * (9 * 2 + 1), 3 * (9 * 2 + 2), 3 * (9 * 2 + 3), 3 * (9 * 2 + SPPNum + 8), Selections);

    BlendPixel(3 * (9 * 2 + 1), 3 * (9 * 2 + 2), 3 * (9 * 2 + 3), 3 * (9 * 2 + 4),3 * (9 * 2 + SPPNum + 9), Selections);
}

float LossSigma = std::sqrt(2.0f / M_PI_FLT);
float InitLoss(float* selections)
{
    int32_t offset = 9 * 3;
    float loss = 0.0f;
    float lossPdf = 1.0f / 9;

    for (int32_t i = 0; i < 9; ++i)
    {
        float Q_r = selections[i * 3], Q_g = selections[i * 3 + 1], Q_b = selections[i * 3 + 2];
        float I_r = selections[i * 3 + offset], I_g = selections[i * 3 + offset + 1], I_b = selections[i * 3 + 2 + offset];
        
        //float Q_lum = Luman(Q_r, Q_g, Q_b);
        //float I_lum = Luman(I_r, I_g, I_b);
        float weight = Gaussian(1 - std::fabs(Q_r - I_r), 0.255) *
            Gaussian(1 - std::fabs(Q_g - I_g), 0.255) *
            Gaussian(1 - std::fabs(Q_b - I_b), 0.255);
        loss += weight;

        
        //loss = std::fabs(Q_lum - I_lum);
        
        //std::cout << "loss " << loss << " " << Q_lum << " " << I_lum << std::endl;
         
    }
    return loss * lossPdf;
}

float LocalLoss(float* selections, int32_t SPP, float* result)
{
    int32_t offset = 9 * 3;

    float averageLum = 0.0;
    float I_lum = 0.0f;

    averageLum = Luminance(result[0], result[1], result[2]);

    float I_R = selections[offset + 4 * 3],
        I_G = selections[offset + 4 * 3 + 1],
        I_B = selections[offset + 4 * 3 + 2];
    I_lum = Luminance(I_R, I_G, I_B);

    float avg_r = 0, avg_g = 0, avg_b = 0, avg_weight = 0;
    float gt_r = 0, gt_g = 0, gt_b = 0, gt_weight = 0;
    for (int32_t i = 0; i < SPP; ++i)
    {
        int32_t LoosOffset = 3 * 9 * 2 + i * 3;
        float Q_r = selections[LoosOffset];
        float Q_g = selections[LoosOffset + 1];
        float Q_b = selections[LoosOffset + 2];

        float SelLum = Luminance(Q_r, Q_g, Q_b);
        float SelWeight = Gaussian(std::fabs(averageLum - SelLum), 0.255);

        avg_r += Q_r * SelWeight;
        avg_g += Q_g * SelWeight;
        avg_b += Q_b * SelWeight;
        avg_weight += SelWeight;

        float GTSelWeight = Gaussian(std::fabs(I_lum - SelLum), 0.7978);
        if (GTSelWeight < 0)
        {
            std::cout << "LocalLoss " << I_lum << " " << SelLum << " " << Gaussian(std::fabs(I_lum - SelLum), 0.7978) <<
                " " << clamp(Gaussian(std::fabs(I_lum - SelLum), 0.7978), 0, 1) << std::endl;
        }
        if (GTSelWeight > gt_weight)
        {
            gt_weight = GTSelWeight;
        }
    }

    avg_r /= avg_weight;
    avg_g /= avg_weight;
    avg_b /= avg_weight;

    if (avg_weight < 0.001)
    {
        result[0] = I_R;
        result[1] = I_G;
        result[2] = I_B;
        return 0;
    }  
    
    //gt_weight = 0;
    result[0] = I_R * gt_weight + (1 - gt_weight) * avg_r;
    result[1] = I_G * gt_weight + (1 - gt_weight) * avg_g;
    result[2] = I_B * gt_weight + (1 - gt_weight) * avg_b;

    //result[0] = avg_r;
    //result[1] = avg_g;
    //result[2] = avg_b;
    //result[0] = I_R;
    //result[1] = I_G;
    //result[2] = I_B;
    return 0;
}


void globalLoss(float* selections, float* result)
{
    int32_t offset = 9 * 3;

    float resultLum = Luminance(result[0], result[1], result[2]);

    float weight = 0.0f;
    float r = 0.0, g = 0.0, b = 0.0;
    for (int32_t i = 0; i < 9; ++i)
    {
        if (i == 4)
        {
            continue;
        }
        float I_r = selections[i * 3 + offset], I_g = selections[i * 3 + offset + 1], I_b = selections[i * 3 + 2 + offset];
        float Q_r = selections[i * 3], Q_g = selections[i * 3 + 1], Q_b = selections[i * 3 + 2];

        float lum = Luminance(I_r, I_g, I_b);
        float QLum = Luminance(Q_r, Q_g, Q_b);
        float tmp_weight = Gaussian(std::fabs(QLum - lum), 0.255);

        r += Q_r * tmp_weight;
        g += Q_g * tmp_weight;
        b += Q_b * tmp_weight;

        weight += tmp_weight;
    }

    if (weight <= 0.001)
    {
        return;
    }

    r /= weight;
    b /= weight;
    g /= weight;
}

float GetLoss(float* selections, int32_t LossIndex)
{
    int32_t offset = 9 * 3;
    float loss = 0.0f;
    float lossPdf = 1.0f / 9;

    for (int32_t i = 0; i < 9; ++i)
    {
        //int32_t i = 4;
        float Q_r = selections[i * 3], Q_g = selections[i * 3 + 1], Q_b = selections[i * 3 + 2];
        
        if (i == 4)
        {
            int32_t LoosOffset = 3 * 9 * 2 + LossIndex * 3;
            Q_r = selections[LoosOffset];
            Q_g = selections[LoosOffset + 1];
            Q_b = selections[LoosOffset + 2];
        }

        float I_r = selections[i * 3 + offset], I_g = selections[i * 3 + offset + 1], I_b = selections[i * 3 + 2 + offset];

        float weight = Gaussian(1 - std::fabs(Q_r - I_r), 0.255) *
            Gaussian(1 - std::fabs(Q_g - I_g), 0.255) *
            Gaussian(1 - std::fabs(Q_b - I_b), 0.255);
        loss += weight;

        //std::cout << "loss " << loss << " " << Q_lum << " " << I_lum << std::endl;

    }
    return loss * lossPdf;
}

constexpr int32_t IteratorMinNum = 1;
constexpr int32_t SelectionNum = 32;
inline void deNoising(int32_t size_x, int32_t size_y, int32_t channel, int32_t SPP, int32_t SPPOffset,
    float* color, float* normal, float* pos, uint8_t* layerData)
{
    std::cout << "--------------DeNoising Begin--------------------" << std::endl;

    uint32_t offset_normal = (uint32_t)(channel * (0 * size_x + 0));

    float* layer = (float*)(layerData + SPPOffset * 1);

    if (layerData == nullptr)
        std::cout << "layerdata is nullptr " << std::endl;

    std::cout << "renderBlock " << SPP << " " <<
        SPPOffset << " " << size_x << " " << size_y << std::endl;

    std::cout << "renderBlock " << *(layer + offset_normal) << " " <<
        *(layer + offset_normal + 1) << std::endl;

    uint8_t* surrogateImage = static_cast<uint8_t*>(allocAligned(SPPOffset));

    //FilterImage(size_x, size_y, channel, color, (float*)surrogateImage, normal, pos);
    FilterImage(size_x, size_y, channel, color, (float*)surrogateImage, normal, pos,true);

    //copyImage(size_x, size_y, channel, (float*)surrogateImage, color);
    //return;

    //iterative min
    std::cout << "iterative min begin" << std::endl;

    uint8_t* Q = static_cast<uint8_t*>(allocAligned(SPPOffset));
    //use one layer to init Q
    copyImage(size_x, size_y,channel,(float*)(layerData), (float*)Q);
    //copyImage(size_x, size_y, channel, (float*)layerData, color);
    //return;

    std::cout << "GetSelection begin" << std::endl;
    int32_t MISPP = 10;
    //3x3kernel
    float selections[3 * (9 * 2 + 64)];
    for (int32_t iter = 0; iter < IteratorMinNum; ++iter)
    {
        for (int32_t i = 0; i < size_x; ++i)
        {
            for (int32_t j = 0; j < size_y; ++j)
            {
                size_t offset = ((size_t)i + size_x * (size_t)j) * channel;
                size_t QIndex = 0;
                for (int32_t m = -1; m <= 1; ++m)
                {
                    for (int32_t n = -1; n <= 1; ++n)
                    {
                        int32_t QOffset_x = clamp(i + m, 0, size_x - 1);
                        int32_t QOffset_y = clamp(j + n, 0, size_y - 1);

                        int32_t QOffset = (QOffset_x + QOffset_y * size_x) * channel;
                        selections[0 + QIndex * 3] = *((float*)(Q)+QOffset);
                        selections[1 + QIndex * 3] = *((float*)(Q)+QOffset + 1);
                        selections[2 + QIndex * 3] = *((float*)(Q)+QOffset + 2);
                        
                        selections[0 + QIndex * 3 + 9 * 3] = *((float*)(surrogateImage)+QOffset);
                        selections[1 + QIndex * 3 + 9 * 3] = *((float*)(surrogateImage)+QOffset + 1);
                        selections[2 + QIndex * 3 + 9 * 3] = *((float*)(surrogateImage)+QOffset + 2);


                        QIndex += 1;
                    }
                }
                //*((float*)(Q)+offset) = layer[offset];
                //*((float*)(Q)+offset + 1) = layer[offset + 1];
                //*((float*)(Q)+offset + 2) = layer[offset + 2];
                
                GetSelection(SPPOffset, SPP, 0, size_x, size_y, channel, i, j,layerData, selections);

                //std::cout << "GetSelection end" << std::endl;

                //float result[3]{ 
                //    (selections[9 * 3 * 2 + 3 * 0 + 0] + selections[9 * 3 * 2 + 3 * 1 + 0] + 
                //    selections[9 * 3 * 2 + 3 * 2 + 0] + selections[9 * 3 * 2 + 3 * 3 + 0])/4.0f,
                //    (selections[9 * 3 * 2 + 3 * 0 + 1] + selections[9 * 3 * 2 + 3 * 1 + 1] +
                //    selections[9 * 3 * 2 + 3 * 2 + 1] + selections[9 * 3 * 2 + 3 * 3 + 1]) / 4.0f ,
                //    (selections[9 * 3 * 2 + 3 * 0 + 2] + selections[9 * 3 * 2 + 3 * 1 + 2] +
                //    selections[9 * 3 * 2 + 3 * 2 + 2] + selections[9 * 3 * 2 + 3 * 3 + 2]) / 4.0f };
                float result[3]{ selections[3 * (9 + 4)],
                    selections[3 * (9 + 4) + 1],
                    selections[3 * (9 + 4) + 2]
                };

                //float loss = InitLoss(selections);
                //float loss = 0.0f;

                //for (int32_t LossIndex = 0; LossIndex < SPP + MISPP; ++LossIndex)
                //{
                //    float LayerLoss = GetLoss(selections, LossIndex);
                //    if (LayerLoss <= loss)
                //    {
                //        //std::cout << "loss " << LayerLoss << " " << loss << std::endl;

                //        loss = LayerLoss;
                //        result[0] = selections[3 * 9 * 2 + LossIndex * 3];
                //        result[1] = selections[3 * 9 * 2 + LossIndex * 3 + 1];
                //        result[2] = selections[3 * 9 * 2 + LossIndex * 3 + 2];
                //    }
                //}
                 
                globalLoss(selections, result);
                LocalLoss(selections, SPP, result);

                *(color + offset) = result[0];
                *(color + offset + 1) = result[1];
                *(color + offset + 2) = result[2];

            }
        }

        //FilterImage(size_x, size_y, channel, (float*)Q, color, normal, pos);
        copyImage(size_x, size_y, channel, color, (float*)Q);
    }

    std::cout << "iterative min end" << std::endl;

    copyImage(size_x, size_y, channel, (float*)Q, color);
    //FilterImage(size_x, size_y, channel, (float*)Q, color, normal, pos);

    freeAligned(surrogateImage);
    freeAligned(Q);

    std::cout << "--------------DeNoising End--------------------" << std::endl;

}