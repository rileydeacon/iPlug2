#include "tape.h"
#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
static inline void XorShift32(uint32_t& state)
{
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
}

class TapeLoFiSVGKnobControl : public IKnobControlBase
{
public:
  TapeLoFiSVGKnobControl(const IRECT& bounds, const ISVG& svg, int paramIdx, float vibe = 1.f)
    : IKnobControlBase(bounds, paramIdx)
    , mSVG(svg)
    , mVibe(vibe)
  {
  }

  void Draw(IGraphics& g) override
  {
    // Snap draw coordinates to reduce sub-pixel shimmer on tiny controls.
    const float cx = std::round(mRECT.MW() * 2.f) * 0.5f;
    const float cy = std::round(mRECT.MH() * 2.f) * 0.5f;
    const float w = std::round(mRECT.W() * 2.f) * 0.5f;
    const float h = std::round(mRECT.H() * 2.f) * 0.5f;
    const double angle = mStartAngle + GetValue() * (mEndAngle - mStartAngle);

    const float blurRadius = std::max(0.35f, std::min(w, h) * 0.03f) * mVibe;
    const float baseWeight = mBlend.mWeight;

    // Symmetric multi-sample blur (gaussian-like) without directional ghosting.
    constexpr int kSamples = 16;
    for (int i = 0; i < kSamples; i++)
    {
      const float theta = ((2.f * kPi) / static_cast<float>(kSamples)) * static_cast<float>(i);
      const float ox = std::cos(theta) * (blurRadius * 0.45f);
      const float oy = std::sin(theta) * (blurRadius * 0.45f);
      const IBlend innerBlend(EBlend::Default, Clip(baseWeight * (0.32f / static_cast<float>(kSamples)), 0.f, 1.f));
      g.DrawRotatedSVG(mSVG, cx + ox, cy + oy, w, h, angle, &innerBlend);
    }

    for (int i = 0; i < kSamples; i++)
    {
      const float theta = ((2.f * kPi) / static_cast<float>(kSamples)) * static_cast<float>(i) + (kPi / static_cast<float>(kSamples));
      const float ox = std::cos(theta) * blurRadius;
      const float oy = std::sin(theta) * blurRadius;
      const IBlend outerBlend(EBlend::Default, Clip(baseWeight * (0.16f / static_cast<float>(kSamples)), 0.f, 1.f));
      g.DrawRotatedSVG(mSVG, cx + ox, cy + oy, w, h, angle, &outerBlend);
    }

    const IBlend mainBlend(EBlend::Default, Clip(baseWeight * 0.78f, 0.f, 1.f));
    g.DrawRotatedSVG(mSVG, cx, cy, w, h, angle, &mainBlend);

    DrawWear(g, cx, cy, w, h, static_cast<float>(angle));
  }

private:
  static constexpr float kPi = 3.14159265358979323846f;

  static uint32_t Hash32(uint32_t x)
  {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
  }

  static float Hash01(int x, int y, int seed)
  {
    uint32_t h = static_cast<uint32_t>(x) * 73856093U;
    h ^= static_cast<uint32_t>(y) * 19349663U;
    h ^= static_cast<uint32_t>(seed) * 83492791U;
    h = Hash32(h);
    return static_cast<float>(h & 0x00FFFFFFU) / 16777215.f;
  }

  void DrawWear(IGraphics& g, float cx, float cy, float w, float h, float angleDegrees) const
  {
    const float r = std::min(w, h) * 0.5f;
    const float angleRadians = angleDegrees * (kPi / 180.f);
    const float c = std::cos(angleRadians);
    const float s = std::sin(angleRadians);
    const int gridStep = (r < 16.f) ? 2 : 3;

    const IColor darkSpeck(18, 22, 10, 0);
    const IColor lightSpeck(14, 255, 240, 214);
    const IColor wearBand(16, 58, 30, 10);
    const IBlend bandBlend(EBlend::Default, Clip(0.34f * mVibe, 0.f, 1.f));

    // Deterministic local-space wear so the "damage" rotates with the knob.
    for (int ly = static_cast<int>(-r) + 2; ly < static_cast<int>(r) - 1; ly += gridStep)
    {
      for (int lx = static_cast<int>(-r) + 2; lx < static_cast<int>(r) - 1; lx += gridStep)
      {
        const float dist2 = static_cast<float>((lx * lx) + (ly * ly));
        if (dist2 >= ((r - 1.5f) * (r - 1.5f)))
          continue;

        const float noise = Hash01(lx, ly, mNoiseSeed);
        if (noise > (0.988f - (0.0025f * mVibe)))
        {
          const float rx = (static_cast<float>(lx) * c) - (static_cast<float>(ly) * s);
          const float ry = (static_cast<float>(lx) * s) + (static_cast<float>(ly) * c);
          g.DrawPoint(lightSpeck, cx + rx, cy + ry);
        }
        else if (noise < (0.012f + (0.0025f * mVibe)))
        {
          const float rx = (static_cast<float>(lx) * c) - (static_cast<float>(ly) * s);
          const float ry = (static_cast<float>(lx) * s) + (static_cast<float>(ly) * c);
          g.DrawPoint(darkSpeck, cx + rx, cy + ry);
        }
      }
    }

    // Subtle scanline-like wear bands.
    for (int i = -1; i <= 1; i += 2)
    {
      const float yLocal = static_cast<float>(i) * r * 0.22f;
      const float span = std::sqrt(std::max(0.f, (r * r) - (yLocal * yLocal))) - 2.f;
      const float x1 = -span;
      const float x2 = span;

      const float rx1 = (x1 * c) - (yLocal * s);
      const float ry1 = (x1 * s) + (yLocal * c);
      const float rx2 = (x2 * c) - (yLocal * s);
      const float ry2 = (x2 * s) + (yLocal * c);

      g.DrawLine(wearBand, cx + rx1, cy + ry1, cx + rx2, cy + ry2, &bandBlend, 1.f);
    }
  }

  ISVG mSVG;
  float mVibe = 1.f;
  float mStartAngle = -135.f;
  float mEndAngle = 135.f;
  int mNoiseSeed = 1337;
};
} // namespace

tape::tape(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // Single knob: Drive starts at 0 (clean bypass).
  GetParam(kParamtapeDrive)->InitDouble("Drive", 0.0, 0.0, 100.0, 0.1, "%", IParam::kFlagsNone);
  UpdateMorphTargets(0.0);
  ResetTapeState();

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, 1.f); };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    constexpr float kMainKnobX = 106.4f;
    constexpr float kMainKnobY = 86.8f;
    constexpr float kMainKnobW = 87.2f;
    constexpr float kMainKnobH = 87.2f;

    const ISVG knobtapeSVG = pGraphics->LoadSVG(TAPE_KNOB_FN);
    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new TapeLoFiSVGKnobControl(IRECT::MakeXYWH(kMainKnobX, kMainKnobY, kMainKnobW, kMainKnobH), knobtapeSVG, kParamtapeDrive, 1.0f));
  };
#endif
}

#if IPLUG_DSP
void tape::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  constexpr double kPi = 3.14159265358979323846;
  const int nOutChans = NOutChansConnected();
  const int nInChans = NInChansConnected();

  if (nOutChans < 1 || nFrames <= 0)
    return;

  SmoothTapeParams();

  const double sampleRate = GetSampleRate();
  const double overallscale = sampleRate / 44100.0;
  const int spacing = std::clamp(static_cast<int>(std::floor(overallscale)), 1, 16);

  const double inputGain = std::pow(mParamA * 2.0, 2.0);
  const double dublyAmount = mParamB * 2.0;
  const double outlyAmount = std::max(-1.0, (1.0 - mParamB) * -2.0);
  const double iirEncFreq = (1.0 - mParamC) / overallscale;
  const double iirDecFreq = mParamC / overallscale;
  const double iirMidFreq = ((mParamC * 0.618) + 0.382) / overallscale;

  double flutDepth = std::pow(mParamD, 6.0) * overallscale * 50.0;
  flutDepth = std::min(498.0, flutDepth);
  const double flutFrequency = (0.02 * std::pow(mParamE, 3.0)) / overallscale;

  const double bias = (mParamF * 2.0) - 1.0;
  double underBias = (std::pow(bias, 4.0) * 0.25) / overallscale;
  double overBias = std::pow(1.0 - bias, 3.0) / overallscale;
  if (bias > 0.0)
    underBias = 0.0;
  if (bias < 0.0)
    overBias = 1.0 / overallscale;

  mGSlew[26] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[23] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[20] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[17] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[14] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[11] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[8] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[5] = overBias;
  overBias *= 1.618033988749894848204586;
  mGSlew[2] = overBias;

  const double headBumpDrive = (mParamG * 0.1) / overallscale;
  const double headBumpMix = mParamG * 0.5;
  const double subCurve = std::sin(mParamG * kPi);
  const double iirSubFreq = (subCurve * 0.008) / overallscale;

  // Head bump biquads
  mHdbA[0] = mParamH / sampleRate;
  mHdbB[0] = mHdbA[0] * 0.9375;
  mHdbA[1] = 0.618033988749894848204586;
  mHdbB[1] = 0.618033988749894848204586;
  mHdbA[3] = 0.0;
  mHdbB[3] = 0.0;

  double K = std::tan(kPi * mHdbA[0]);
  double norm = 1.0 / (1.0 + K / mHdbA[1] + K * K);
  mHdbA[2] = K / mHdbA[1] * norm;
  mHdbA[4] = -mHdbA[2];
  mHdbA[5] = 2.0 * (K * K - 1.0) * norm;
  mHdbA[6] = (1.0 - K / mHdbA[1] + K * K) * norm;

  K = std::tan(kPi * mHdbB[0]);
  norm = 1.0 / (1.0 + K / mHdbB[1] + K * K);
  mHdbB[2] = K / mHdbB[1] * norm;
  mHdbB[4] = -mHdbB[2];
  mHdbB[5] = 2.0 * (K * K - 1.0) * norm;
  mHdbB[6] = (1.0 - K / mHdbB[1] + K * K) * norm;

  // Auto gain compensation (smoothed in SmoothTapeParams).
  const double autoGain = mAutoGainComp;

  for (int s = 0; s < nFrames; s++)
  {
    double inputSampleL = (nInChans > 0) ? inputs[0][s] : 0.0;
    double inputSampleR = (nInChans > 1) ? inputs[1][s] : inputSampleL;

    if (std::fabs(inputSampleL) < 1.18e-23)
      inputSampleL = static_cast<double>(mFPDL) * 1.18e-17;
    if (std::fabs(inputSampleR) < 1.18e-23)
      inputSampleR = static_cast<double>(mFPDR) * 1.18e-17;

    if (inputGain != 1.0)
    {
      inputSampleL *= inputGain;
      inputSampleR *= inputGain;
    }

    // Dubly encode
    mIirEncL = (mIirEncL * (1.0 - iirEncFreq)) + (inputSampleL * iirEncFreq);
    double highPart = (inputSampleL - mIirEncL) * 2.848;
    highPart += mAvgEncL;
    mAvgEncL = (inputSampleL - mIirEncL) * 1.152;
    highPart = std::clamp(highPart, -1.0, 1.0);
    double dubly = std::fabs(highPart);
    if (dubly > 0.0)
    {
      const double adjust = std::log(1.0 + (255.0 * dubly)) / 2.40823996531;
      if (adjust > 0.0)
        dubly /= adjust;
      mCompEncL = (mCompEncL * (1.0 - iirEncFreq)) + (dubly * iirEncFreq);
      inputSampleL += ((highPart * mCompEncL) * dublyAmount);
    }

    mIirEncR = (mIirEncR * (1.0 - iirEncFreq)) + (inputSampleR * iirEncFreq);
    highPart = (inputSampleR - mIirEncR) * 2.848;
    highPart += mAvgEncR;
    mAvgEncR = (inputSampleR - mIirEncR) * 1.152;
    highPart = std::clamp(highPart, -1.0, 1.0);
    dubly = std::fabs(highPart);
    if (dubly > 0.0)
    {
      const double adjust = std::log(1.0 + (255.0 * dubly)) / 2.40823996531;
      if (adjust > 0.0)
        dubly /= adjust;
      mCompEncR = (mCompEncR * (1.0 - iirEncFreq)) + (dubly * iirEncFreq);
      inputSampleR += ((highPart * mCompEncR) * dublyAmount);
    }

    // Flutter (kept consistent with CurioTapeSaturator)
    if (flutDepth > 0.0)
    {
      if (mGCount < 0 || mGCount > 999)
        mGCount = 999;

      mDelayL[mGCount] = inputSampleL;
      int count = mGCount;
      double offset = flutDepth + (flutDepth * std::sin(mSweepL));
      mSweepL += mNextMaxL * flutFrequency;
      if (mSweepL > (kPi * 2.0))
      {
        mSweepL -= (kPi * 2.0);
        XorShift32(mFPDL);
        const double flutA = 0.24 + (static_cast<double>(mFPDL) / static_cast<double>(UINT32_MAX) * 0.74);
        XorShift32(mFPDL);
        const double flutB = 0.24 + (static_cast<double>(mFPDL) / static_cast<double>(UINT32_MAX) * 0.74);
        mNextMaxL = (std::fabs(flutA - std::sin(mSweepR + mNextMaxR)) < std::fabs(flutB - std::sin(mSweepR + mNextMaxR))) ? flutA : flutB;
      }

      int iOffset = static_cast<int>(std::floor(offset));
      double dOffset = offset - std::floor(offset);
      // Match ToTape8 read-head direction: advance from gcount by offset.
      int idx1 = count + iOffset;
      while (idx1 < 0)
        idx1 += 1000;
      while (idx1 >= 1000)
        idx1 -= 1000;
      int idx2 = idx1 + 1;
      while (idx2 < 0)
        idx2 += 1000;
      while (idx2 >= 1000)
        idx2 -= 1000;
      inputSampleL = (mDelayL[idx1] * (1.0 - dOffset)) + (mDelayL[idx2] * dOffset);

      mDelayR[mGCount] = inputSampleR;
      count = mGCount;
      offset = flutDepth + (flutDepth * std::sin(mSweepR));
      mSweepR += mNextMaxR * flutFrequency;
      if (mSweepR > (kPi * 2.0))
      {
        mSweepR -= (kPi * 2.0);
        XorShift32(mFPDR);
        const double flutA = 0.24 + (static_cast<double>(mFPDR) / static_cast<double>(UINT32_MAX) * 0.74);
        XorShift32(mFPDR);
        const double flutB = 0.24 + (static_cast<double>(mFPDR) / static_cast<double>(UINT32_MAX) * 0.74);
        mNextMaxR = (std::fabs(flutA - std::sin(mSweepL + mNextMaxL)) < std::fabs(flutB - std::sin(mSweepL + mNextMaxL))) ? flutA : flutB;
      }

      iOffset = static_cast<int>(std::floor(offset));
      dOffset = offset - std::floor(offset);
      idx1 = count + iOffset;
      while (idx1 < 0)
        idx1 += 1000;
      while (idx1 >= 1000)
        idx1 -= 1000;
      idx2 = idx1 + 1;
      while (idx2 < 0)
        idx2 += 1000;
      while (idx2 >= 1000)
        idx2 -= 1000;
      inputSampleR = (mDelayR[idx1] * (1.0 - dOffset)) + (mDelayR[idx2] * dOffset);

      mGCount -= 1;
    }

    // Bias
    if (std::fabs(bias) > 0.001)
    {
      for (int x = 0; x < 27; x += 3)
      {
        if (underBias > 0.0)
        {
          double stuck = std::fabs(inputSampleL - (mGSlew[x] / 0.975)) / underBias;
          if (stuck < 1.0)
            inputSampleL = (inputSampleL * stuck) + ((mGSlew[x] / 0.975) * (1.0 - stuck));

          stuck = std::fabs(inputSampleR - (mGSlew[x + 1] / 0.975)) / underBias;
          if (stuck < 1.0)
            inputSampleR = (inputSampleR * stuck) + ((mGSlew[x + 1] / 0.975) * (1.0 - stuck));
        }

        if ((inputSampleL - mGSlew[x]) > mGSlew[x + 2])
          inputSampleL = mGSlew[x] + mGSlew[x + 2];
        if (-(inputSampleL - mGSlew[x]) > mGSlew[x + 2])
          inputSampleL = mGSlew[x] - mGSlew[x + 2];
        mGSlew[x] = inputSampleL * 0.975;

        if ((inputSampleR - mGSlew[x + 1]) > mGSlew[x + 2])
          inputSampleR = mGSlew[x + 1] + mGSlew[x + 2];
        if (-(inputSampleR - mGSlew[x + 1]) > mGSlew[x + 2])
          inputSampleR = mGSlew[x + 1] - mGSlew[x + 2];
        mGSlew[x + 1] = inputSampleR * 0.975;
      }
    }

    // ToTape bands
    mIirMidRollerL = (mIirMidRollerL * (1.0 - iirMidFreq)) + (inputSampleL * iirMidFreq);
    double highsSampleL = inputSampleL - mIirMidRollerL;
    double lowsSampleL = mIirMidRollerL;
    if (iirSubFreq > 0.0)
    {
      mIirLowCutoffL = (mIirLowCutoffL * (1.0 - iirSubFreq)) + (lowsSampleL * iirSubFreq);
      lowsSampleL -= mIirLowCutoffL;
    }
    lowsSampleL = std::clamp(lowsSampleL, -1.57079633, 1.57079633);
    lowsSampleL = std::sin(lowsSampleL);
    double thinnedHigh = std::fabs(highsSampleL) * 1.57079633;
    thinnedHigh = std::min(1.57079633, thinnedHigh);
    thinnedHigh = 1.0 - std::cos(thinnedHigh);
    if (highsSampleL < 0.0)
      thinnedHigh = -thinnedHigh;
    highsSampleL -= thinnedHigh;

    mIirMidRollerR = (mIirMidRollerR * (1.0 - iirMidFreq)) + (inputSampleR * iirMidFreq);
    double highsSampleR = inputSampleR - mIirMidRollerR;
    double lowsSampleR = mIirMidRollerR;
    if (iirSubFreq > 0.0)
    {
      mIirLowCutoffR = (mIirLowCutoffR * (1.0 - iirSubFreq)) + (lowsSampleR * iirSubFreq);
      lowsSampleR -= mIirLowCutoffR;
    }
    lowsSampleR = std::clamp(lowsSampleR, -1.57079633, 1.57079633);
    lowsSampleR = std::sin(lowsSampleR);
    thinnedHigh = std::fabs(highsSampleR) * 1.57079633;
    thinnedHigh = std::min(1.57079633, thinnedHigh);
    thinnedHigh = 1.0 - std::cos(thinnedHigh);
    if (highsSampleR < 0.0)
      thinnedHigh = -thinnedHigh;
    highsSampleR -= thinnedHigh;

    // Head bump
    double headBumpSampleL = 0.0;
    double headBumpSampleR = 0.0;
    if (headBumpMix > 0.0)
    {
      mHeadBumpL += (lowsSampleL * headBumpDrive);
      mHeadBumpL -= (mHeadBumpL * mHeadBumpL * mHeadBumpL * (0.0618 / std::sqrt(overallscale)));
      mHeadBumpR += (lowsSampleR * headBumpDrive);
      mHeadBumpR -= (mHeadBumpR * mHeadBumpR * mHeadBumpR * (0.0618 / std::sqrt(overallscale)));

      const double headBiqSampleL = (mHeadBumpL * mHdbA[2]) + mHdbA[7];
      mHdbA[7] = (mHeadBumpL * mHdbA[3]) - (headBiqSampleL * mHdbA[5]) + mHdbA[8];
      mHdbA[8] = (mHeadBumpL * mHdbA[4]) - (headBiqSampleL * mHdbA[6]);

      headBumpSampleL = (headBiqSampleL * mHdbB[2]) + mHdbB[7];
      mHdbB[7] = (headBiqSampleL * mHdbB[3]) - (headBumpSampleL * mHdbB[5]) + mHdbB[8];
      mHdbB[8] = (headBiqSampleL * mHdbB[4]) - (headBumpSampleL * mHdbB[6]);

      const double headBiqSampleR = (mHeadBumpR * mHdbA[2]) + mHdbA[9];
      mHdbA[9] = (mHeadBumpR * mHdbA[3]) - (headBiqSampleR * mHdbA[5]) + mHdbA[10];
      mHdbA[10] = (mHeadBumpR * mHdbA[4]) - (headBiqSampleR * mHdbA[6]);

      headBumpSampleR = (headBiqSampleR * mHdbB[2]) + mHdbB[9];
      mHdbB[9] = (headBiqSampleR * mHdbB[3]) - (headBumpSampleR * mHdbB[5]) + mHdbB[10];
      mHdbB[10] = (headBiqSampleR * mHdbB[4]) - (headBumpSampleR * mHdbB[6]);
    }

    inputSampleL = lowsSampleL + highsSampleL + (headBumpSampleL * headBumpMix);
    inputSampleR = lowsSampleR + highsSampleR + (headBumpSampleR * headBumpMix);

    // Dubly decode
    mIirDecL = (mIirDecL * (1.0 - iirDecFreq)) + (inputSampleL * iirDecFreq);
    highPart = (inputSampleL - mIirDecL) * 2.628;
    highPart += mAvgDecL;
    mAvgDecL = (inputSampleL - mIirDecL) * 1.372;
    highPart = std::clamp(highPart, -1.0, 1.0);
    dubly = std::fabs(highPart);
    if (dubly > 0.0)
    {
      const double adjust = std::log(1.0 + (255.0 * dubly)) / 2.40823996531;
      if (adjust > 0.0)
        dubly /= adjust;
      mCompDecL = (mCompDecL * (1.0 - iirDecFreq)) + (dubly * iirDecFreq);
      inputSampleL += ((highPart * mCompDecL) * outlyAmount);
    }

    mIirDecR = (mIirDecR * (1.0 - iirDecFreq)) + (inputSampleR * iirDecFreq);
    highPart = (inputSampleR - mIirDecR) * 2.628;
    highPart += mAvgDecR;
    mAvgDecR = (inputSampleR - mIirDecR) * 1.372;
    highPart = std::clamp(highPart, -1.0, 1.0);
    dubly = std::fabs(highPart);
    if (dubly > 0.0)
    {
      const double adjust = std::log(1.0 + (255.0 * dubly)) / 2.40823996531;
      if (adjust > 0.0)
        dubly /= adjust;
      mCompDecR = (mCompDecR * (1.0 - iirDecFreq)) + (dubly * iirDecFreq);
      inputSampleR += ((highPart * mCompDecR) * outlyAmount);
    }

    // ClipOnly2
    inputSampleL = std::clamp(inputSampleL, -4.0, 4.0);
    if (mWasPosClipL)
    {
      if (inputSampleL < mLastSampleL)
        mLastSampleL = 0.7058208 + (inputSampleL * 0.2609148);
      else
        mLastSampleL = 0.2491717 + (mLastSampleL * 0.7390851);
    }
    mWasPosClipL = false;
    if (inputSampleL > 0.9549925859)
    {
      mWasPosClipL = true;
      inputSampleL = 0.7058208 + (mLastSampleL * 0.2609148);
    }

    if (mWasNegClipL)
    {
      if (inputSampleL > mLastSampleL)
        mLastSampleL = -0.7058208 + (inputSampleL * 0.2609148);
      else
        mLastSampleL = -0.2491717 + (mLastSampleL * 0.7390851);
    }
    mWasNegClipL = false;
    if (inputSampleL < -0.9549925859)
    {
      mWasNegClipL = true;
      inputSampleL = -0.7058208 + (mLastSampleL * 0.2609148);
    }

    mIntermediateL[spacing] = inputSampleL;
    inputSampleL = mLastSampleL;
    for (int x = spacing; x > 0; x--)
      mIntermediateL[x - 1] = mIntermediateL[x];
    mLastSampleL = mIntermediateL[0];

    inputSampleR = std::clamp(inputSampleR, -4.0, 4.0);
    if (mWasPosClipR)
    {
      if (inputSampleR < mLastSampleR)
        mLastSampleR = 0.7058208 + (inputSampleR * 0.2609148);
      else
        mLastSampleR = 0.2491717 + (mLastSampleR * 0.7390851);
    }
    mWasPosClipR = false;
    if (inputSampleR > 0.9549925859)
    {
      mWasPosClipR = true;
      inputSampleR = 0.7058208 + (mLastSampleR * 0.2609148);
    }

    if (mWasNegClipR)
    {
      if (inputSampleR > mLastSampleR)
        mLastSampleR = -0.7058208 + (inputSampleR * 0.2609148);
      else
        mLastSampleR = -0.2491717 + (mLastSampleR * 0.7390851);
    }
    mWasNegClipR = false;
    if (inputSampleR < -0.9549925859)
    {
      mWasNegClipR = true;
      inputSampleR = -0.7058208 + (mLastSampleR * 0.2609148);
    }

    mIntermediateR[spacing] = inputSampleR;
    inputSampleR = mLastSampleR;
    for (int x = spacing; x > 0; x--)
      mIntermediateR[x - 1] = mIntermediateR[x];
    mLastSampleR = mIntermediateR[0];

    // Internal tape output compensation (part of the original algorithm design).
    const double tapeOutputComp = mParamI * 2.0;
    inputSampleL *= tapeOutputComp;
    inputSampleR *= tapeOutputComp;

    // Auto gain compensation (replaces separate output knob)
    inputSampleL *= autoGain;
    inputSampleR *= autoGain;

    // Light dither (kept from CurioTapeSaturator)
    XorShift32(mFPDL);
    inputSampleL += ((static_cast<double>(mFPDL) - static_cast<double>(0x7fffffff)) * 5.5e-36 * std::pow(2.0, 62.0));
    XorShift32(mFPDR);
    inputSampleR += ((static_cast<double>(mFPDR) - static_cast<double>(0x7fffffff)) * 5.5e-36 * std::pow(2.0, 62.0));

    outputs[0][s] = static_cast<sample>(inputSampleL);
    if (nOutChans > 1)
      outputs[1][s] = static_cast<sample>(inputSampleR);
  }
}

void tape::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
  case kParamtapeDrive:
    mtapeDriveValue = GetParam(kParamtapeDrive)->Value() / 100.0;
    UpdateMorphTargets(mtapeDriveValue);
    break;
  default:
    break;
  }
}

void tape::OnReset() { ResetTapeState(); }

void tape::UpdateMorphTargets(double morph)
{
  const double knob = std::clamp(morph, 0.0, 1.0);

  // CurioTapeSaturator setMorph mapping.
  // v1.1: Uses correct parameter values from user's working patch.
  // CRITICAL FIX: Shape (C) goes to 1.0 which disables the dubly encode filter,
  // preventing the high frequency loss ("filter sweep").
  mTargetA = 0.5 + (knob * 0.5);   // Input: 0.5 -> 1.0
  mTargetB = 0.5 - (knob * 0.173); // Tilt: 0.5 -> 0.327
  mTargetC = 0.5 + (knob * 0.5);   // Shape: 0.5 -> 1.0
  mTargetD = knob * 0.300;         // Flutter: 0.0 -> 0.300
  mTargetE = 0.200;                // FlutSpd: locked at 0.200
  mTargetF = 0.5 + (knob * 0.320); // Bias: 0.5 -> 0.820
  mTargetG = 0.5 + (knob * 0.042); // HeadBmp: 0.5 -> 0.542
  mTargetH = 50.0 + (knob * 44.5); // HeadFrq: 50.0 -> 94.5
  mTargetI = 0.5;                  // Output comp: stays at 0.5 (unity)

  // Loudness compensation for the single Drive macro.
  // This keeps perceived track level more uniform as saturation density rises.
  constexpr double kMaxCompDb = -6.0;   // attenuation at full Drive
  constexpr double kCompCurve = 1.35;   // ease-in so low Drive remains mostly untouched
  const double compDb = kMaxCompDb * std::pow(knob, kCompCurve);
  mTargetAutoGain = std::pow(10.0, compDb / 20.0);
}

void tape::SmoothTapeParams()
{
  // Match CurioTape block-smoothing behavior.
  // v1.1: Accelerated smoothing so the complex morph transition doesn't hang in
  // weird intermediate states (which can sound like a heavy filter).
  mParamA = (mParamA * 0.5) + (mTargetA * 0.5);
  mParamB = (mParamB * 0.5) + (mTargetB * 0.5);
  mParamC = (mParamC * 0.5) + (mTargetC * 0.5);
  mParamD = (mParamD * 0.5) + (mTargetD * 0.5);
  mParamE = (mParamE * 0.5) + (mTargetE * 0.5);
  mParamF = (mParamF * 0.5) + (mTargetF * 0.5);
  mParamG = (mParamG * 0.5) + (mTargetG * 0.5);
  mParamH = (mParamH * 0.5) + (mTargetH * 0.5);
  mParamI = (mParamI * 0.5) + (mTargetI * 0.5);
  mAutoGainComp = (mAutoGainComp * 0.5) + (mTargetAutoGain * 0.5);
}

void tape::ResetTapeState()
{
  mIirEncL = mIirDecL = 0.0;
  mCompEncL = mCompDecL = 1.0;
  mAvgEncL = mAvgDecL = 0.0;
  mIirEncR = mIirDecR = 0.0;
  mCompEncR = mCompDecR = 1.0;
  mAvgEncR = mAvgDecR = 0.0;

  mDelayL.fill(0.0);
  mDelayR.fill(0.0);
  mSweepL = 3.14159265358979323846;
  mSweepR = 3.14159265358979323846;
  mNextMaxL = 0.5;
  mNextMaxR = 0.5;
  mGCount = 0;

  mGSlew.fill(0.0);
  mIirMidRollerL = mIirLowCutoffL = 0.0;
  mIirMidRollerR = mIirLowCutoffR = 0.0;

  mHeadBumpL = 0.0;
  mHeadBumpR = 0.0;
  mHdbA.fill(0.0);
  mHdbB.fill(0.0);

  mLastSampleL = 0.0;
  mWasPosClipL = false;
  mWasNegClipL = false;
  mLastSampleR = 0.0;
  mWasPosClipR = false;
  mWasNegClipR = false;
  mIntermediateL.fill(0.0);
  mIntermediateR.fill(0.0);

  if (mFPDL < 16386U)
    mFPDL = 0xBADF00DU;
  if (mFPDR < 16386U)
    mFPDR = 0xDEADBEEFU;
  XorShift32(mFPDL);
  XorShift32(mFPDR);
}
#endif
