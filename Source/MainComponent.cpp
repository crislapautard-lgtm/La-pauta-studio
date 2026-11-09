#include "MainComponent.h"
MainComponent::MainComponent()
{
    setSize(1280, 720); setAudioChannels(2, 2); formatManager.addDefaultFormats(); startTimer(50);
    juce::Colour cols[4] = { juce::Colours::red, juce::Colours::orange, juce::Colours::green, juce::Colours::skyblue };
    for(int i=0;i<4;++i){ Track t; t.name="Pista "+juce::String(i+1); t.color=cols[i]; tracks.add(t); }
    addAndMakeVisible(titleLabel); titleLabel.setText("LA PAUTA STUDIO - COMO REAPER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold)); titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(playButton); playButton.setButtonText("▶ PLAY");
    addAndMakeVisible(stopButton); stopButton.setButtonText("■ STOP");
    addAndMakeVisible(recButton); recButton.setButtonText("🔴 REC VOZ");
    addAndMakeVisible(loadSampleButton); loadSampleButton.setButtonText("CARGAR SAMPLE");
    addAndMakeVisible(renderButton); renderButton.setButtonText("RENDER WAV");
    addAndMakeVisible(scanButton); scanButton.setButtonText("SCAN VST3");
    addAndMakeVisible(saveButton); saveButton.setButtonText("GUARDAR PROYECTO");
    addAndMakeVisible(loadButton); loadButton.setButtonText("ABRIR PROYECTO");
    addAndMakeVisible(addTrackButton); addTrackButton.setButtonText("+ PISTA");
    addAndMakeVisible(metronomeButton); metronomeButton.setButtonText("METRONOMO");
    addAndMakeVisible(beatButton); beatButton.setButtonText("BEAT MAKER");
    addAndMakeVisible(mixerButton); mixerButton.setButtonText("MIXER");
    playButton.onClick=[this]{play();}; stopButton.onClick=[this]{stop();}; recButton.onClick=[this]{toggleRec();};
    loadSampleButton.onClick=[this]{loadSample();}; renderButton.onClick=[this]{renderMix();};
    scanButton.onClick=[this]{scanVST3();}; saveButton.onClick=[this]{saveProject();}; loadButton.onClick=[this]{loadProject();};
    addTrackButton.onClick=[this]{addTrack();}; metronomeButton.onClick=[this]{toggleMetronome();};
    beatButton.onClick=[this]{openBeatMaker();}; mixerButton.onClick=[this]{openMixer();};
    addAndMakeVisible(statusLabel); statusLabel.setText("Listo. Elige pista, graba voz, carga sample, guarda proyecto.", juce::dontSendNotification); statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmLabel); bpmLabel.setText("BPM", juce::dontSendNotification); bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmSlider); bpmSlider.setRange(60,200,1); bpmSlider.setValue(95); bpmSlider.onValueChange=[this]{bpm=(int)bpmSlider.getValue();};
    addAndMakeVisible(masterSlider); masterSlider.setRange(0,1,0.01); masterSlider.setValue(0.8);
    addAndMakeVisible(trackSelector); for(int i=0;i<tracks.size();++i) trackSelector.addItem(tracks[i].name, i+1); trackSelector.setSelectedId(1); trackSelector.onChange=[this]{selectedTrack=trackSelector.getSelectedId()-1;};
    addAndMakeVisible(eqLabel); eqLabel.setText("EQ LOW / MID / HIGH", juce::dontSendNotification); eqLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(eqLowSlider); eqLowSlider.setRange(-12,12,0.1); eqLowSlider.setValue(0); eqLowSlider.onValueChange=[this]{eqLow=(float)eqLowSlider.getValue();};
    addAndMakeVisible(eqMidSlider); eqMidSlider.setRange(-12,12,0.1); eqMidSlider.setValue(0); eqMidSlider.onValueChange=[this]{eqMid=(float)eqMidSlider.getValue();};
    addAndMakeVisible(eqHighSlider); eqHighSlider.setRange(-12,12,0.1); eqHighSlider.setValue(0); eqHighSlider.onValueChange=[this]{eqHigh=(float)eqHighSlider.getValue();};
}
MainComponent::~MainComponent(){ shutdownAudio(); stopTimer(); }
void MainComponent::timerCallback(){ repaint(); }
void MainComponent::play(){ isPlaying=true; for(auto &t: tracks) t.pos=0; playhead=0; statusLabel.setText("Sonando "+juce::String(tracks.size())+" pistas", juce::dontSendNotification); }
void MainComponent::stop(){ isPlaying=false; }
void MainComponent::addTrack(){ Track t; t.name="Pista "+juce::String(tracks.size()+1); t.color=juce::Colours::purple; tracks.add(t); trackSelector.addItem(t.name, tracks.size()); statusLabel.setText("Nueva: "+t.name, juce::dontSendNotification); resized(); }
void MainComponent::toggleMetronome(){ metronomeOn=!metronomeOn; statusLabel.setText(metronomeOn?"Metronomo ON "+juce::String(bpm)+" BPM":"Metronomo OFF", juce::dontSendNotification); }
void MainComponent::openBeatMaker(){ juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"BEAT MAKER","DEMBOW: Kick - Kick - Snare\nTRAP: 808 + HiHat\nDRILL: 808 deslizado + Snare\n\nCarga tus drums con CARGAR SAMPLE en cada pista.", "DALE"); }
void MainComponent::openMixer(){ juce::String m; for(auto &t: tracks) m+=t.name+" | Vol "+juce::String((int)(t.volume*100))+"% | "+(t.muted?"MUTE":"ON")+"\n"; juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"MIXER",m,"OK"); }
void MainComponent::toggleRec(){
    if(!isRecording){
        auto docs=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File f=docs.getChildFile("LaPauta_Voz.wav"); if(f.exists()) f.deleteFile();
        juce::WavAudioFormat wav;
        if(auto s=f.createOutputStream()){ writer.reset(wav.createWriterFor(s.release(), sampleRate, 2, 16, {}, 0)); isRecording=true; isPlaying=false; recButton.setButtonText("⏹ PARAR"); statusLabel.setText("🔴 GRABANDO VOZ en "+tracks[selectedTrack].name+" - Canta ahora!", juce::dontSendNotification); }
    } else {
        isRecording=false; writer.reset(); recButton.setButtonText("🔴 REC VOZ");
        auto docs=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        loadFileIntoBuffer(docs.getChildFile("LaPauta_Voz.wav"), selectedTrack);
        statusLabel.setText("Voz guardada en "+tracks[selectedTrack].name+" - Dale PLAY pa' escucharte", juce::dontSendNotification);
    }
}
void MainComponent::loadSample(){
    fileChooser=std::make_unique<juce::FileChooser>("Elige sample WAV MP3 Kick 808 Loop", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav;*.mp3;*.aif;*.flac");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& fc){ auto file=fc.getResult(); if(file.exists()){ currentSampleFile=file; loadFileIntoBuffer(file, selectedTrack); statusLabel.setText("Sample "+file.getFileName()+" en "+tracks[selectedTrack].name, juce::dontSendNotification);} });
}
void MainComponent::loadFileIntoBuffer(juce::File file, int idx){
    if(idx<0||idx>=tracks.size()) return;
    if(auto* r=formatManager.createReaderFor(file)){ tracks.getReference(idx).buffer.setSize((int)r->numChannels,(int)r->lengthInSamples); r->read(&tracks.getReference(idx).buffer,0,(int)r->lengthInSamples,0,true,true); tracks.getReference(idx).hasAudio=true; tracks.getReference(idx).pos=0; delete r; }
}
void MainComponent::scanVST3(){
    juce::StringArray folders; folders.add("C:\\Program Files\\Common Files\\VST3"); folders.add("C:\\Program Files\\VSTPlugins");
    foundPlugins.clear();
    for(auto &f: folders){ juce::File folder(f); if(folder.exists()){ juce::Array<juce::File> files; folder.findChildFiles(files, juce::File::findFiles, true, "*.vst3"); for(auto &file: files) foundPlugins.add(file.getFileNameWithoutExtension()); } }
    juce::String r="ENCONTRE "+juce::String(foundPlugins.size())+" PLUGINS:\n\n"; for(auto &n: foundPlugins) r+="✅ "+n+"\n";
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"SCAN VST3",r,"OK");
}
void MainComponent::renderMix(){
    auto docs=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File outFile=docs.getChildFile("LaPauta_Mix_Final.wav");
    juce::WavAudioFormat wav; if(auto s=outFile.createOutputStream()){ if(auto* w=wav.createWriterFor(s.release(), sampleRate, 2, 16, {}, 0)){ int total=0; for(auto &t: tracks) total=juce::jmax(total, t.buffer.getNumSamples()); juce::AudioSampleBuffer mix(2,total); mix.clear(); for(auto &t: tracks){ if(!t.hasAudio||t.muted) continue; for(int ch=0;ch<2;++ch){ int src=juce::jmin(ch, t.buffer.getNumChannels()-1); mix.addFrom(ch,0,t.buffer,src,0,total,t.volume); } } w->writeFromAudioSampleBuffer(mix,0,total); delete w; } }
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"RENDER","Listo! Documentos / LaPauta_Mix_Final.wav - Todas tus pistas mezcladas.", "CORONAMOS");
}
void MainComponent::saveProject(){
    fileChooser=std::make_unique<juce::FileChooser>("Guardar proyecto", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MiTema.lapauta"), "*.lapauta");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode,[this](const juce::FileChooser& fc){ auto file=fc.getResult(); if(file.getFileName().isEmpty()) return; if(file.getFileExtension()!=".lapauta") file=file.withFileExtension(".lapauta"); juce::DynamicObject::Ptr o=new juce::DynamicObject(); o->setProperty("bpm", bpmSlider.getValue()); o->setProperty("master", masterSlider.getValue()); o->setProperty("sampleFile", currentSampleFile.getFullPathName()); juce::FileOutputStream out(file); if(out.openedOk()){ out.writeText(juce::JSON::toString(juce::var(o.get())), false,false,"\n"); statusLabel.setText("Proyecto guardado: "+file.getFileName(), juce::dontSendNotification);} });
}
void MainComponent::loadProject(){
    fileChooser=std::make_unique<juce::FileChooser>("Abrir proyecto", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.lapauta");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& fc){ auto file=fc.getResult(); if(!file.exists()) return; auto j=juce::JSON::parse(file.loadFileAsString()); if(j.isObject()){ auto* o=j.getDynamicObject(); if(o!=nullptr){ bpmSlider.setValue((double)o->getProperty("bpm"), juce::dontSendNotification); masterSlider.setValue((double)o->getProperty("master"), juce::dontSendNotification); juce::String p=o->getProperty("sampleFile").toString(); if(p.isNotEmpty()){ currentSampleFile=juce::File(p); if(currentSampleFile.exists()) loadFileIntoBuffer(currentSampleFile,0); } statusLabel.setText("Proyecto abierto: "+file.getFileNameWithoutExtension(), juce::dontSendNotification);} } });
}
void MainComponent::paint(juce::Graphics& g){
    g.fillAll(juce::Colours::white); g.setColour(juce::Colour(0,180,180)); g.fillRect(0,0,getWidth(),60);
    g.setColour(juce::Colours::lightgrey); g.fillRect(0,60,getWidth(),50);
    int y=130;
    for(int i=0;i<tracks.size();++i){ auto &t=tracks[i]; g.setColour(i==selectedTrack?juce::Colours::yellow:t.color); g.fillRect(0,y,200,60); g.setColour(juce::Colours::black); g.drawText(t.name,10,y+5,180,20,juce::Justification::left); g.drawText(t.hasAudio?"🔊 Audio listo":"Sin audio - REC o Cargar",10,y+30,180,20,juce::Justification::left); g.setColour(juce::Colours::darkgrey); g.fillRect(200,y,getWidth()-200,60); if(t.hasAudio){ g.setColour(t.color); float w=(float)getWidth()-220; float prog=t.buffer.getNumSamples()>0?(float)t.pos/(float)t.buffer.getNumSamples():0; g.fillRect(210,y+15,(int)(w*prog+10),30); } if(isPlaying){ g.setColour(juce::Colours::red); g.fillRect(200+playhead%(getWidth()-200), y, 2, 60); } y+=65; }
    g.setColour(juce::Colours::darkgrey); g.fillRect(0,getHeight()-90,getWidth(),90);
}
void MainComponent::resized(){
    titleLabel.setBounds(15,10,400,40);
    playButton.setBounds(450,15,80,30); stopButton.setBounds(535,15,80,30); recButton.setBounds(620,15,110,30); metronomeButton.setBounds(735,15,120,30);
    scanButton.setBounds(15,70,130,30); beatButton.setBounds(150,70,120,30); mixerButton.setBounds(275,70,90,30);
    loadSampleButton.setBounds(370,70,160,30); saveButton.setBounds(535,70,160,30); loadButton.setBounds(700,70,140,30);
    renderButton.setBounds(845,70,120,30); addTrackButton.setBounds(970,70,70,30);
    bpmLabel.setBounds(870,15,40,30); bpmSlider.setBounds(910,15,120,30); masterSlider.setBounds(1050,15,100,30);
    trackSelector.setBounds(15,getHeight()-80,200,25);
    eqLabel.setBounds(230,getHeight()-80,180,25); eqLowSlider.setBounds(410,getHeight()-80,100,25); eqMidSlider.setBounds(520,getHeight()-80,100,25); eqHighSlider.setBounds(630,getHeight()-80,100,25);
    statusLabel.setBounds(15,getHeight()-50,getWidth()-30,30);
}
void MainComponent::prepareToPlay(int s, double sr){ sampleRate=sr; }
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info){
    if(info.buffer==nullptr) return; auto* out=info.buffer;
    if(isRecording && writer!=nullptr) writer->writeFromAudioSampleBuffer(*out,0,info.numSamples);
    out->clear();
    if(isPlaying){
        for(auto &t: tracks){ if(!t.hasAudio||t.muted) continue; int toCopy=juce::jmin(info.numSamples, t.buffer.getNumSamples()-t.pos); if(toCopy>0){ for(int ch=0;ch<out->getNumChannels();++ch){ int src=juce::jmin(ch, t.buffer.getNumChannels()-1); out->addFrom(ch,0,t.buffer,src,t.pos,toCopy,t.volume); } t.pos+=toCopy; } }
        playhead+=info.numSamples;
    }
    float gain=(float)masterSlider.getValue() * (1.0f + eqHigh*0.02f);
    out->applyGain(gain);
}
void MainComponent::releaseResources(){}
