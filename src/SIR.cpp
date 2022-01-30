#include "plugin.hpp"

#define MAX_BUFFER_SIZE 32

struct SIR : Module {
	enum ParamId {
        BPM_PARAM,
        TIME_PARAM,
		INITIAL_INFECTED_PARAM,
		INFECTION_RATE_PARAM,
		RECOVERY_RATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INITIAL_INFECTED_INPUT,
		TRIGGER_INPUT,
		TIME_INPUT,
		INFECTION_RATE_INPUT,
		RECOVERY_RATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
        SUSCEPTIBLE_OUTPUT,
        INFECTED_OUTPUT,
		RECOVERED_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
    
    int initialSusceptible = 98;
    int initialInfected = 2;
    const int initialRecovered = 0;
    float infectionRate;
    float recoveryRate;
    float t = 0.0;
    float dt = 0.0001;
    float kFactor = 1.0;
    
    std::vector<float> SIRpoints = {(float) initialSusceptible, (float) initialInfected, (float) initialRecovered};

    float frameRate = 60.f;
    int samplesPerFrame;
    int sampleCounter = 0;

    
    float plotBuffer[4][MAX_BUFFER_SIZE] = {};
    int bufferIndex = 0;
    
    dsp::SchmittTrigger clockTrigger;

	SIR() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(BPM_PARAM, 0.f, 1.f, 0.f, "Int. Clock");
		configParam(TIME_PARAM, 0.5f, 2.f, 1.f, "KFactor");
		configParam(INITIAL_INFECTED_PARAM, 1.f, 10.f, 1.f, "Initial Infected");
		configParam(INFECTION_RATE_PARAM, 0.f, 0.5f, 0.1f, "Infection Rate");
		configParam(RECOVERY_RATE_PARAM, 0.f, 5.f, 1.f, "Recovery Rate");
        configInput(TRIGGER_INPUT, "Ext. Clock");
        configInput(TIME_INPUT, "");
		configInput(INITIAL_INFECTED_INPUT, "Initial Infected");
		configInput(INFECTION_RATE_INPUT, "Infection Rate");
		configInput(RECOVERY_RATE_INPUT, "Recovery Rate");
        configOutput(SUSCEPTIBLE_OUTPUT, "No. Susceptible");
        configOutput(INFECTED_OUTPUT, "No. Infected");
		configOutput(RECOVERED_OUTPUT, "No. Recovered");
	}
    
    void onAdd(const AddEvent &e) override
    {
        samplesPerFrame=(1.f/frameRate)*APP->engine->getSampleRate();
//        sampleCheck = samplesPerFrame - 400;
    }

    void onSampleRateChange(const SampleRateChangeEvent &e) override
    {
        samplesPerFrame=(1.f/frameRate)*APP->engine->getSampleRate();
//        sampleCheck = samplesPerFrame;
    }
    
    void process(const ProcessArgs& args) override
    {
        if (inputs[TRIGGER_INPUT].isConnected())
        {
            if (clockTrigger.process(inputs[TRIGGER_INPUT].getVoltage()))
            {
                if (inputs[INITIAL_INFECTED_INPUT].isConnected())
                    initialInfected = std::max(1, (int) std::round(inputs[INITIAL_INFECTED_INPUT].getVoltage()));
                else
                    initialInfected = std::max(1, (int) std::round(params[INITIAL_INFECTED_PARAM].getValue()));
                
                initialSusceptible = 100 - initialInfected;
                
                SIRpoints = {(float) initialSusceptible, (float) initialInfected, (float) initialRecovered};
                t = 0.f;
                
                bufferIndex = 0;
                for (int i = 0; i < MAX_BUFFER_SIZE; ++i)
                {
                    for (int j = 1; j < 4; ++j)
                    {
                        plotBuffer[j][i] = 0.f;
                    }
                }
                
            }
            
            updateParams();
            
            
            SIRpoints = getNewSIRPoints(SIRpoints, infectionRate, recoveryRate, kFactor);
            if (sampleCounter >= samplesPerFrame)
            {
                sampleCounter = 0;
                updateBuffer();
            }
            sampleCounter++;
            outputs[SUSCEPTIBLE_OUTPUT].setVoltage(clamp(SIRpoints[0]/10.f, 0.f, 10.f));
            outputs[INFECTED_OUTPUT].setVoltage(clamp(SIRpoints[1]/10.f, 0.f, 10.f));
            outputs[RECOVERED_OUTPUT].setVoltage(clamp(SIRpoints[2]/10.f, 0.f, 10.f));
        }
        
        
        
    } // process
        
    std::vector<float> getNewSIRPoints(std::vector<float> points, float iRate, float rRate, float k)
    {
        std::vector<float> output = {0.0, 0.0, 0.0};
        float dS = -1*iRate*points[0]*points[1];
        float dI = iRate*points[0]*points[1] - rRate*points[1];
        float dR = rRate*points[1];
        output[0] = points[0] + dS * dt*k;
        output[1] = points[1] + dI * dt*k;
        output[2] = points[2] + dR * dt*k;
        t += dt;
        return output;
    }
    
    void updateParams()
    {
        if (inputs[INFECTION_RATE_INPUT].isConnected())
            infectionRate = inputs[INFECTION_RATE_INPUT].getVoltage() / 20.f;
        else
            infectionRate = params[INFECTION_RATE_INPUT].getValue();
        if (inputs[RECOVERY_RATE_INPUT].isConnected())
            recoveryRate = inputs[RECOVERY_RATE_INPUT].getVoltage() / 2.f;
        else
            recoveryRate = params[RECOVERY_RATE_INPUT].getValue();
        kFactor = params[TIME_INPUT].getValue();
    }
    
    void updateBuffer()
    {
        if (bufferIndex >= MAX_BUFFER_SIZE)
            bufferIndex = 0;
        plotBuffer[0][bufferIndex] = t;
        for (int i = 1; i < 4; ++i)
        {
            plotBuffer[i][bufferIndex] = SIRpoints[i-1];
        }
        bufferIndex++;

    }
};

// Implement Display Here
struct SIRDisplay : TransparentWidget
{
    SIR *module;
    
    
    const int dAlpha = 256/MAX_BUFFER_SIZE;
    
    float normalizeX (float x) {return x/4.f * box.size.x;}
    
    float normalizeY (float y) {return (y/100.f) * box.size.y;}
    
    void drawCurve (const DrawArgs &args, int line, NVGcolor color)
    {
        int bufIdx = module->bufferIndex;
        for (int i = 0; i < MAX_BUFFER_SIZE - 1; ++i)
        {
            if (bufIdx == 0)
                bufIdx = MAX_BUFFER_SIZE - 1;
            bufIdx -= 1;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, normalizeX(module->plotBuffer[0][bufIdx]), box.size.y - normalizeY(module->plotBuffer[line][bufIdx]));
            nvgLineTo(args.vg, normalizeX(module->plotBuffer[0][bufIdx - 1]), box.size.y - normalizeY(module->plotBuffer[line][bufIdx - 1]));
            nvgStrokeColor(args.vg, nvgTransRGBA(color, 255 - (i*dAlpha)));
            nvgStrokeWidth(args.vg, clamp(3.0 - ((float) i*0.15), 0.1f, 3.0f));
            nvgStroke(args.vg);
        }
        
    }
    
    void draw(const DrawArgs &args) override
    {
        nvgFillColor(args.vg, nvgRGB(20, 20, 20));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFill(args.vg);
    }
    
    void drawLayer(const DrawArgs &args, int layer) override
    {
        if (module == NULL) return;
        
        if (layer == 1)
        {
            nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
            
            drawCurve(args, 1, nvgRGB(0, 0, 200)); // Susceptible
            drawCurve(args, 2, nvgRGB(200, 0, 0)); // Infected
            drawCurve(args, 3, nvgRGB(0, 200, 0)); // Recovered
            
        }
        Widget::drawLayer(args, layer);
    }
    
    
}; // SIRDisplay

struct SIRWidget : ModuleWidget {
	SIRWidget(SIR* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SIR.svg")));

        {
            auto display = new SIRDisplay();
            display->module = module;
            display->box.pos = mm2px(Vec(2.0, 62.789));
            display->box.size = mm2px(Vec(56.96, 44.0));
            addChild(display);
            display->show();
        }
        
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.178, 33.936)), module, SIR::BPM_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(19.052, 34.309)), module, SIR::TIME_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.611, 34.095)), module, SIR::INITIAL_INFECTED_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(42.599, 33.666)), module, SIR::INFECTION_RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(54.801, 34.095)), module, SIR::RECOVERY_RATE_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.178, 20.936)), module, SIR::TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.956, 20.936)), module, SIR::TIME_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.784, 21.15)), module, SIR::INITIAL_INFECTED_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.451, 20.936)), module, SIR::INFECTION_RATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(54.711, 20.936)), module, SIR::RECOVERY_RATE_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.724, 120.562)), module, SIR::SUSCEPTIBLE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.974, 120.252)), module, SIR::INFECTED_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(51.645, 120.466)), module, SIR::RECOVERED_OUTPUT));

//        auto display = new SIRDisplay(Vec(2.0, 62.789), Vec(56.96, 44.0));
//        display->module = module;
//        addChild(display);
    }
};


Model* modelSIR = createModel<SIR, SIRWidget>("SIR");
