#include <cstdio>  // for printing to stdout

#include "al/app/al_App.hpp"
#include "al/math/al_Random.hpp"
#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Gamma.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Types.h"
#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

using namespace gam;
using namespace al;
using namespace std;

class PluckedString : public SynthVoice {
public:
    float mAmp;
    float mDur;
    float mPanRise;
    gam::Pan<> mPan;
    gam::NoisePink<> noise;//example plucking: https://www.youtube.com/watch?v=ROe61GyvtXM&t=90s
    gam::Decay<> env;
    gam::MovingAvg<> fil {2};
    gam::Delay<float, gam::ipl::Trunc> delay;
    gam::ADSR<> mAmpEnv;
    gam::EnvFollow<> mEnvFollow;
    gam::Env<2> mPanEnv;

    // Additional members
    Mesh mMesh;

    virtual void init(){
        mAmp  = 1;
        mDur = 2;
        mAmpEnv.levels(0, 1, 1, 0);
        mPanEnv.curve(4);
        env.decay(0.1);
        delay.maxDelay(1./27.5);
        delay.delay(1./440.0);


        addCube(mMesh, 1.0, 3);
        createInternalTriggerParameter("amplitude", 0.6, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 0.001, 0.001, 1.0);
        createInternalTriggerParameter("releaseTime", 0.25, 0.1, 10.0);
        createInternalTriggerParameter("sustain", 0.5, 0.0, 1.0);
        createInternalTriggerParameter("Pan1", 0.0, -1.0, 1.0);
        createInternalTriggerParameter("Pan2", 0.0, -1.0, 1.0);
        createInternalTriggerParameter("PanRise", 0.0, -1.0, 1.0); // range check
    }
    
    float operator() (){
        return (*this)(noise()*env());
    }
    float operator() (float in){
        return delay(
                     fil( delay() + in )
                     );
    }

    float last = 0;
    virtual void onProcess(AudioIOData& io) override {
        while(io()){
            mPan.pos(mPanEnv());
            float s1 =  (*this)() * mAmpEnv() * mAmp;
            s1 = (s1 + last) / 2; //todo
            last = s1;
            float s2;
            mEnvFollow(s1);
            mPan(s1, s1,s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        if(mAmpEnv.done() && (mEnvFollow.value() < 0.001)) free();

    }

    virtual void onProcess(Graphics &g) {
          float frequency = getInternalParameterValue("frequency");
          float amplitude = getInternalParameterValue("amplitude");
          g.pushMatrix();
          g.translate(amplitude,  amplitude, -4);
          //g.scale(frequency/2000, frequency/4000, 1);
          float scaling = 0.1;
          g.scale(scaling * frequency/200, scaling * frequency/400, scaling* 1);
          g.color( frequency/1000, mEnvFollow.value(), frequency/2000, 0.4);
          g.draw(mMesh);
          g.popMatrix();
    }
 
    virtual void onTriggerOn() override {
        updateFromParameters();
        mAmpEnv.reset();
        env.reset();
        delay.zero();
    }

    virtual void onTriggerOff() override {
        mAmpEnv.triggerRelease();
    }

    void updateFromParameters() {
        mPanEnv.levels(getInternalParameterValue("Pan1"),
                       getInternalParameterValue("Pan2"),
                       getInternalParameterValue("Pan1"));
        mPanRise = getInternalParameterValue("PanRise");
        delay.freq(getInternalParameterValue("frequency"));
        mAmp = getInternalParameterValue("amplitude");
        mAmpEnv.levels()[1] = 1.0;
        mAmpEnv.levels()[2] = getInternalParameterValue("sustain");
        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mAmpEnv.lengths()[3] = getInternalParameterValue("releaseTime");

        mPanEnv.lengths()[0] = mDur * (1-mPanRise);
        mPanEnv.lengths()[1] = mDur * mPanRise;
    }

};


struct Particle {
  Vec3f pos, vel, acc;
  int age = 0;

  void update(int ageInc) {
    vel += acc;
    pos += vel;
    age += ageInc;
  }
};

class MyApp : public App
{
public:
  SynthGUIManager<PluckedString> synthManager {"plunk"};
  //    ParameterMIDI parameterMIDI;

  virtual void onInit( ) override {
    imguiInit();
    navControl().active(false);  // Disable navigation via keyboard, since we
                              // will be using keyboard for note triggering
    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
  }

    void onCreate() override {
        // Play example sequence. Comment this line to start from scratch
        //    synthManager.synthSequencer().playSequence("synth8.synthSequence");
        nav().pullBack(20);
        synthManager.synthRecorder().verbose(true);
    }

    template <int N>
    struct Emitter {
      Particle particles[N];
      int tap = 0;

      Emitter() {
        for (auto& p : particles) p.age = N;
      }

      template <int M>
      void update(float xpos, float ypos) {
        for (auto& p : particles) p.update(M);

        for (int i = 0; i < M; ++i) {
          auto& p = particles[tap];

          // fountain
          if (al::rnd::prob(0.95)) {
            p.vel.set(al::rnd::uniform(0.2), 0,
                      0);

            p.acc.set(0, -0.03, 0);
          }
          p.pos.set(xpos, ypos, 0);

          p.age = 0;
          ++tap;
          if (tap >= N) tap = 0;
        }
      }

      int size() { return N; }
    };

    void onSound(AudioIOData& io) override {
        synthManager.render(io);  // Render audio
    }

    Emitter<250> em1;
    Emitter<250> em2;
    Emitter<250> em3;
    Emitter<250> em4;
    Emitter<250> em5;
    Emitter<250> em6;
    Emitter<250> em7;
    Emitter<250> em8;
    Emitter<250> em9;
    Emitter<250> em10;
    Emitter<250> em11;
    Emitter<250> em12;
    Emitter<250> em13;
    Emitter<250> em14;
    Emitter<250> em15;
    Emitter<250> em16;
    Emitter<250> em17;
    Emitter<250> em18;
    Emitter<250> em21;
    Emitter<250> em22;
    Emitter<250> em23;
    Emitter<250> em24;
    Emitter<250> em25;
    Emitter<250> em26;
    Emitter<250> em27;
    Emitter<250> em28;
    Emitter<250> em29;
    Emitter<250> em30;
    Emitter<250> em31;
    Emitter<250> em32;
    Emitter<250> em33;
    Emitter<250> em34;
    Emitter<250> em35;
    Emitter<250> em36;
    Emitter<250> em37;
    Emitter<250> em38;
    Mesh mesh;


    void computeParticles(Emitter<250> em) {
        for (int i = 0; i < em.size(); ++i) {
        Particle& p = em.particles[i];
        float age = float(p.age) / em.size();

        mesh.vertex(p.pos);
        mesh.color(HSV(0.6, al::rnd::uniform(), (1 - age) * 0.4));
        }
    }

    void onAnimate(double dt) override {
        // imguiBeginFrame();
        // synthManager.drawSynthControlPanel();
        // imguiEndFrame();
        int height = 6;
        int width = -5;

        em1.update<5>(-4 + width, 0.0 + height);
        em2.update<5>(-3.5 + width, 0.1 + height);
        em3.update<5>(-3 + width, 0.2 + height);
        em4.update<5>(-2.5 + width, 0.3 + height);
        em5.update<5>(-2 + width, 0.4 + height);
        em6.update<5>(-1.5 + width, 0.3 + height);
        em7.update<5>(-1 + width, 0.2 + height);
        em8.update<5>(-0.5 + width, 0.1 + height);
        em9.update<5> (0 + width, 0.0 + height);
        em10.update<5>(0.5 + width, 0.1 + height);
        em11.update<5>(1 + width, 0.2 + height);
        em12.update<5>(1.5 + width, 0.3 + height);
        em13.update<5>(2 + width, 0.4 + height);
        em14.update<5>(2.5 + width, 0.3 + height);
        em15.update<5>(3 + width, 0.2 + height);
        em16.update<5>(3.5 + width, 0.1 + height);
        em17.update<5>(4 + width, 0.0 + height);
        em18.update<5>(4.5 + width, 0.1 + height);

        float offset = 8;
        em21.update<5>(-4 + offset + width, 0.0 + height);
        em22.update<5>(-3.5 + offset + width, 0.1 + height);
        em23.update<5>(-3 + offset + width, 0.2 + height);
        em24.update<5>(-2.5 + offset + width, 0.3 + height);
        em25.update<5>(-2 + offset + width, 0.4 + height);
        em26.update<5>(-1.5 + offset + width, 0.3 + height);
        em27.update<5>(-1 + offset + width, 0.2 + height);
        em28.update<5>(-0.5 + offset + width, 0.1 + height);
        em29.update<5> (0 + offset + width, 0.0 + height);
        em30.update<5>(0.5 + offset + width, 0.1 + height);
        em31.update<5>(1 + offset + width, 0.2 + height);
        em32.update<5>(1.5 + offset + width, 0.3 + height);
        em33.update<5>(2 + offset + width, 0.4 + height);
        em34.update<5>(2.5 + offset + width, 0.3 + height);
        em35.update<5>(3 + offset + width, 0.2 + height);
        em36.update<5>(3.5 + offset + width, 0.1 + height);
        em37.update<5>(4 + offset + width, 0.0 + height);
        em38.update<5>(4.5 + offset + width, 0.1 + height);

        mesh.reset();
        mesh.primitive(Mesh::POINTS);

        computeParticles(em1);
        computeParticles(em2);
        computeParticles(em3);
        computeParticles(em4);
        computeParticles(em5);
        computeParticles(em6);
        computeParticles(em7);
        computeParticles(em8);
        computeParticles(em9);
        computeParticles(em10);
        computeParticles(em11);
        computeParticles(em12);
        computeParticles(em13);
        computeParticles(em14);
        computeParticles(em15);
        computeParticles(em16);
        computeParticles(em17);
        computeParticles(em18);
        computeParticles(em21);
        computeParticles(em22);
        computeParticles(em23);
        computeParticles(em24);
        computeParticles(em25);
        computeParticles(em26);
        computeParticles(em27);
        computeParticles(em28);
        computeParticles(em29);
        computeParticles(em30);
        computeParticles(em31);
        computeParticles(em32);
        computeParticles(em33);
        computeParticles(em34);
        computeParticles(em35);
        computeParticles(em36);
        computeParticles(em37);
        computeParticles(em38);
    }

    void onDraw(Graphics& g) override {
        // g.clear();
        // synthManager.render(g);
        g.clear(0);
        g.blending(true);
        g.blendAdd();
        g.pointSize(6);
        g.meshColor();
        g.draw(mesh);

        // // Draw GUI
        // imguiDraw();
    }

    bool onKeyDown(Keyboard const& k) override {
        if (ParameterGUI::usingKeyboard()) {  // Ignore keys if GUI is using them
        return true;
        }
        if (k.shift()) {
        // If shift pressed then keyboard sets preset
        int presetNumber = asciiToIndex(k.key());
        synthManager.recallPreset(presetNumber);
        } else {
        // Otherwise trigger note for polyphonic synth
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0) {
            synthManager.voice()->setInternalParameterValue(
                "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
            synthManager.triggerOn(midiNote);
        }
        }
        return true;
    }

    bool onKeyUp(Keyboard const& k) override {
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0) {
        synthManager.triggerOff(midiNote);
        }
        return true;
    }

  void onExit() override { imguiShutdown(); }
};

int main() {
  MyApp app;

  // Set up audio
  app.configureAudio(48000., 512, 2, 0);

  app.start();
}
