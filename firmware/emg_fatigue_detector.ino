#include <Arduino.h>
#include <arduinoFFT.h>

#define ADC_PIN        A0
#define BUFFER_SIZE    256
#define SAMPLING_FREQ  1000

unsigned long lastSampleTime = 0;

double rms_smooth = 0;
double mdf_smooth = 0;

double baseline_mdf = 0;
int baseline_count = 0;
bool fatigued = false;

uint16_t bufferA[BUFFER_SIZE];
uint16_t bufferB[BUFFER_SIZE];

volatile bool useBufferA = true;
volatile int bufferIndex = 0;
volatile bool bufferReady = false;

//fft
ArduinoFFT<double> FFT;
double vReal[BUFFER_SIZE];
double vImag[BUFFER_SIZE];
double powerSpectrum[BUFFER_SIZE / 2];

// filter
double hpf_x1=0,hpf_x2=0,hpf_y1=0,hpf_y2=0;
double lpf_x1=0,lpf_x2=0,lpf_y1=0,lpf_y2=0;
double notch_x1=0,notch_x2=0,notch_y1=0,notch_y2=0;

double HPF(double x){
  double y = 0.9391*x - 1.8782*hpf_x1 + 0.9391*hpf_x2 + 1.8731*hpf_y1 - 0.8783*hpf_y2;
  hpf_x2 = hpf_x1; hpf_x1 = x;
  hpf_y2 = hpf_y1; hpf_y1 = y;
  return y;
}

double LPF(double x){
  double y = 0.2066*x + 0.4131*lpf_x1 + 0.2066*lpf_x2 + 0.3695*lpf_y1 - 0.1958*lpf_y2;
  lpf_x2 = lpf_x1; lpf_x1 = x;
  lpf_y2 = lpf_y1; lpf_y1 = y;
  return y;
}

double NOTCH(double x){
  double y = 0.994*x -1.902*notch_x1 +0.994*notch_x2 +1.902*notch_y1 -0.988*notch_y2;
  notch_x2 = notch_x1; notch_x1 = x;
  notch_y2 = notch_y1; notch_y1 = y;
  return y;
}

//rms
double computeRMS(double *data){
  double sum=0;
  for(int i=0;i<BUFFER_SIZE;i++) sum+=data[i]*data[i];
  return sqrt(sum/BUFFER_SIZE);
}

// power
void computePower(){
  for(int i=0;i<BUFFER_SIZE/2;i++)
    powerSpectrum[i] = vReal[i];
  powerSpectrum[0]=0;
}

// mdf
double computeMDF(){
  double total=0;
  for(int i=1;i<BUFFER_SIZE/2;i++) total+=powerSpectrum[i];
  if(total==0) return 0;

  double half=total/2, cum=0;
  for(int i=1;i<BUFFER_SIZE/2;i++){
    cum+=powerSpectrum[i];
    if(cum>=half)
      return (double)i*SAMPLING_FREQ/BUFFER_SIZE;
  }
  return 0;
}

void setup(){
  Serial.begin(921600);
  analogReadResolution(12);
}

void loop(){

  // SAMPLING 
  if(micros() - lastSampleTime >= 1000){
    lastSampleTime += 1000;

    uint16_t s = analogRead(ADC_PIN);

    if(useBufferA) bufferA[bufferIndex]=s;
    else bufferB[bufferIndex]=s;

    bufferIndex++;

    if(bufferIndex>=BUFFER_SIZE){
      bufferIndex=0;
      bufferReady=true;
    }
  }

  if(!bufferReady) return;
  bufferReady=false;

  bool current=useBufferA;
  useBufferA=!useBufferA;

  uint16_t *data = current?bufferA:bufferB;

  //  DC removal 
  double mean=0;
  for(int i=0;i<BUFFER_SIZE;i++) mean+=data[i];
  mean/=BUFFER_SIZE;

  for(int i=0;i<BUFFER_SIZE;i++){
    double x = data[i]-mean;
    x = HPF(x);
    x = LPF(x);
    x = NOTCH(x);

    vReal[i]=x;
    vImag[i]=0;
  }

  //feature
  double rms = computeRMS(vReal);
  rms_smooth = 0.9*rms_smooth + 0.1*rms;

  FFT.windowing(vReal, BUFFER_SIZE, FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(vReal, vImag, BUFFER_SIZE, FFTDirection::Forward);
  FFT.complexToMagnitude(vReal, vImag, BUFFER_SIZE);

  computePower();

  double mdf = computeMDF();
  mdf_smooth = 0.9*mdf_smooth + 0.1*mdf;

  // baseline
  if(baseline_count < 50){
    baseline_mdf += mdf_smooth;
    baseline_count++;

    if(baseline_count == 50){
      baseline_mdf /= 50;
    }
  }

  // det.logic
  if(baseline_count >= 50){
    if(mdf_smooth < 0.7 * baseline_mdf){
      fatigued = true;
    } else {
      fatigued = false;
    }
  }

  
  Serial.print("{");

  Serial.print("\"rms\":"); Serial.print(rms_smooth);
  Serial.print(",\"mdf\":"); Serial.print(mdf_smooth);
  Serial.print(",\"fatigue\":"); Serial.print(fatigued ? 1 : 0);

  Serial.print(",\"raw\":[");
  for(int i=0;i<BUFFER_SIZE;i++){
    Serial.print(data[i]);
    if(i<BUFFER_SIZE-1) Serial.print(",");
  }
  Serial.print("]");

  Serial.print(",\"filt\":[");
  for(int i=0;i<BUFFER_SIZE;i++){
    Serial.print(vReal[i]);
    if(i<BUFFER_SIZE-1) Serial.print(",");
  }
  Serial.print("]");

  Serial.print(",\"fft\":[");
  for(int i=0;i<BUFFER_SIZE/2;i++){
    Serial.print(powerSpectrum[i]);
    if(i<BUFFER_SIZE/2-1) Serial.print(",");
  }
  Serial.print("]");

  Serial.println("}");
}
