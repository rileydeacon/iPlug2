#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "config.h"
#include <array>
#include <cstdint>

const int kNumPresets = 1;

enum EParams
{
  kParamtapeDrive = 0,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class tape final : public Plugin
{
public:
  tape(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnParamChange(int paramIdx) override;
  void OnReset() override;
#endif

private:
  void UpdateMorphTargets(double morph);
  void SmoothTapeParams();
  void ResetTapeState();

  // UI parameters
  double mtapeDriveValue = 0.0; // 0..1 (starts at 0 = clean bypass)

  // Auto gain compensation (computed from morph position)
  double mAutoGainComp = 1.0;
  double mTargetAutoGain = 1.0;

  // CurioTape parameter state
  double mParamA = 0.5;  // Input
  double mParamB = 0.5;  // Tilt
  double mParamC = 0.5;  // Shape
  double mParamD = 0.0;  // Flutter
  double mParamE = 0.2;  // FlutSpd
  double mParamF = 0.5;  // Bias
  double mParamG = 0.5;  // HeadBmp
  double mParamH = 50.0; // HeadFrq
  double mParamI = 0.5;  // Output compensation

  double mTargetA = 0.5;
  double mTargetB = 0.5;
  double mTargetC = 0.5;
  double mTargetD = 0.0;
  double mTargetE = 0.2;
  double mTargetF = 0.5;
  double mTargetG = 0.5;
  double mTargetH = 50.0;
  double mTargetI = 0.5;

  // ToTape8/CurioTape DSP state
  double mIirEncL = 0.0;
  double mIirDecL = 0.0;
  double mCompEncL = 1.0;
  double mCompDecL = 1.0;
  double mAvgEncL = 0.0;
  double mAvgDecL = 0.0;

  double mIirEncR = 0.0;
  double mIirDecR = 0.0;
  double mCompEncR = 1.0;
  double mCompDecR = 1.0;
  double mAvgEncR = 0.0;
  double mAvgDecR = 0.0;

  std::array<double, 1002> mDelayL{};
  std::array<double, 1002> mDelayR{};
  double mSweepL = 3.14159265358979323846;
  double mSweepR = 3.14159265358979323846;
  double mNextMaxL = 0.5;
  double mNextMaxR = 0.5;
  int mGCount = 0;

  std::array<double, 28> mGSlew{};

  double mIirMidRollerL = 0.0;
  double mIirLowCutoffL = 0.0;
  double mIirMidRollerR = 0.0;
  double mIirLowCutoffR = 0.0;

  double mHeadBumpL = 0.0;
  double mHeadBumpR = 0.0;
  std::array<double, 12> mHdbA{};
  std::array<double, 12> mHdbB{};

  double mLastSampleL = 0.0;
  bool mWasPosClipL = false;
  bool mWasNegClipL = false;
  double mLastSampleR = 0.0;
  bool mWasPosClipR = false;
  bool mWasNegClipR = false;
  std::array<double, 17> mIntermediateL{};
  std::array<double, 17> mIntermediateR{};

  uint32_t mFPDL = 0xBADF00D;
  uint32_t mFPDR = 0xDEADBEEF;
};
