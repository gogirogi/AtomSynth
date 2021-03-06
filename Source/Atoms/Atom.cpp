/*
 * Atom.cpp
 *
 *  Created on: Jul 22, 2016
 *      Author: josh
 */

#include "Atom.h"

#include <array>
#include <iostream>

#include "Technical/SaveState.h"
#include "Technical/Synth.h"

namespace AtomSynth {

AtomParameters::AtomParameters(int id, int numPrimaryInputs, bool automationEnabled, int numOutputs) :
		m_numPrimaryInputs(numPrimaryInputs),
		m_numOutputs(numOutputs),
		m_id(id),
		m_automationEnabled(automationEnabled) {

}

AtomParameters AtomParameters::withId(int newId) {
	AtomParameters toReturn = *this;
	toReturn.m_id = newId;
	return toReturn;
}

void AtomController::init() {
	for (int i = 0; i < Synth::getInstance()->getParameters().m_polyphony; i++) {
		m_atoms.push_back(createAtom(i));
	}
}

AtomController::AtomController(AtomParameters parameters) :
		m_primaryInputs(std::vector<std::pair<AtomController *, int>>()),
		m_automationInputs(std::vector<std::pair<AtomController *, int>>()),
		m_atoms(std::vector<Atom *>()),
		m_parameters(parameters),
		m_x(0),
		m_y(0),
		m_stopped(false),
		m_shouldBeDeleted(false),
		m_gui() {
	m_primaryInputs.resize(m_parameters.m_numPrimaryInputs, std::pair<AtomController *, int>(nullptr, 0));
	m_automationInputs.resize(AUTOMATION_INPUTS, std::pair<AtomController *, int>(nullptr, 0));
}

AtomController::~AtomController() {
	for(auto atom : m_atoms) {
		delete(atom);
	}
}

Atom * AtomController::createAtom(int index) {
	return new Atom(*this, index);
}

Atom * AtomController::getAtom(int index) {
	return m_atoms[index];
}

void AtomController::linkPrimaryInput(int index, AtomController* controller, int outputIndex) {
	m_primaryInputs[index].first = controller;
	m_primaryInputs[index].second = outputIndex;
	int atomIndex = 0;
	for (Atom * atom : m_atoms) {
		atom->linkPrimaryInput(index, controller->getAtom(atomIndex)->getOutput(outputIndex));
		atomIndex++;
	}
}

void AtomController::linkAutomationInput(int index, AtomController* controller, int outputIndex) {
	m_automationInputs[index].first = controller;
	m_automationInputs[index].second = outputIndex;
	int atomIndex = 0;
	for (Atom * atom : m_atoms) {
		atom->linkAutomationInput(index, controller->getAtom(atomIndex)->getOutput(outputIndex));
		atomIndex++;
	}
}

void AtomController::linkInput(int index, AtomController * controller, int outputIndex) {
	if (index < m_parameters.m_numPrimaryInputs) {
		linkPrimaryInput(index, controller, outputIndex);
	} else {
		linkAutomationInput(index - m_parameters.m_numPrimaryInputs, controller, outputIndex);
	}
}

void AtomController::unlinkPrimaryInput(int index) {
	m_primaryInputs[index].first = nullptr;
	for (Atom * atom : m_atoms) {
		atom->unlinkPrimaryInput(index);
	}
}

void AtomController::unlinkAutomationInput(int index) {
	m_automationInputs[index].first = nullptr;
	for (Atom * atom : m_atoms) {
		atom->unlinkAutomationInput(index);
	}
}

void AtomController::unlinkInput(int index) {
	if (index < m_parameters.m_numPrimaryInputs) {
		unlinkPrimaryInput(index);
	} else {
		unlinkAutomationInput(index - m_parameters.m_numPrimaryInputs);
	}
}

std::pair<AtomController*, int> AtomController::getInput(int index) {
	if (index < m_parameters.m_numPrimaryInputs) {
		return getPrimaryInput(index);
	} else {
		return getAutomationInput(index - m_parameters.m_numPrimaryInputs);
	}
}

std::vector<std::pair<AtomController*, int> > AtomController::getAllInputs() {
	std::vector<std::pair<AtomController*, int> > toReturn;
	for (std::pair<AtomController*, int> input : m_primaryInputs) {
		toReturn.push_back(input);
	}
	for (std::pair<AtomController*, int> input : m_automationInputs) {
		toReturn.push_back(input);
	}
	return toReturn;
}

void AtomController::loadSaveState(SaveState state) {
	m_x = int(state.getValue(0));
	m_y = int(state.getValue(1));
}

SaveState AtomController::saveSaveState() {
	SaveState state = SaveState();
	state.addValue(m_x);
	state.addValue(m_y);
	return state;
}

void AtomController::execute() {
	if(Synth::getInstance()->getLogManager().shouldDebugEverything()) {
		Synth::getInstance()->getLogManager().addLabel(getName());
			Synth::getInstance()->getLogManager().addLabel("Position");
				Synth::getInstance()->getLogManager().writeInt(m_x);
				Synth::getInstance()->getLogManager().writeInt(m_y);
			Synth::getInstance()->getLogManager().endLabel();
	}

	for (int i = 0; i < Synth::getInstance()->getParameters().m_polyphony; i++) {
		if (Synth::getInstance()->getNoteManager().isActive(i)) { //Only bother calculating active notes
			m_atoms[i]->executeWrapper();
		} else if (Synth::getInstance()->getNoteManager().isStopped(i)) {
			m_atoms[i]->reset();
		}
	}

	if(Synth::getInstance()->getLogManager().shouldDebugEverything()) {
		Synth::getInstance()->getLogManager().endLabel();
	}
}

void AtomController::stopControlAnimation() {
	m_automation.stopAutomationAnimation();
}

void AtomController::cleanupInputsFromAtom(AtomController * source) {
	for (std::pair<AtomController *, int> & input : m_primaryInputs) {
		if (input.first == source) {
			input.first = nullptr;
			input.second = -1;
		}
	}

	for (std::pair<AtomController *, int> & input : m_automationInputs) {
		if (input.first == source) {
			input.first = nullptr;
			input.second = -1;
		}
	}
}

void IOSet::clear() {
	m_constInputs.clear();
	m_incInputs.clear();
	m_outputs.clear();
	m_incInputSources.clear();
	m_constInputSources.clear();
	m_outputSources.clear();
}

DVecIter* IOSet::addInput(AudioBuffer* input) {
	if (input == nullptr) {
		return nullptr;
	} else {
		if (input->isConstant()) { //begin returns a temporary value.
			m_constInputs.push_back(new DVecIter(input->getData().begin()));
			m_constInputSources.push_back(input);
			return m_constInputs.back();
		} else {
			m_incInputs.push_back(new DVecIter(input->getData().begin()));
			m_incInputSources.push_back(input);
			return m_incInputs.back();
		}
	}
}

DVecIter& IOSet::addOutput(AudioBuffer& output) {
	m_outputSources.push_back(&output);
	m_outputs.push_back(new DVecIter(output.getData().begin()));
	return *m_outputs.back();
}

void IOSet::resetPosition() {
	for (int i = 0; i < m_incInputSources.size(); i++)
		(*m_incInputs[i]) = DVecIter(m_incInputSources[i]->getData().begin());
	for (int i = 0; i < m_constInputSources.size(); i++)
		(*m_constInputs[i]) = DVecIter(m_constInputSources[i]->getData().begin());
	for (int i = 0; i < m_outputs.size(); i++)
		(*m_outputs[i]) = DVecIter(m_outputSources[i]->getData().begin());
}

void IOSet::incrementPosition() {
	for (auto iter : m_incInputs)
		(*iter)++;
	for (auto iter : m_outputs)
		(*iter)++;
}

void IOSet::incrementChannel() {
	for (auto iter : m_constInputs)
		(*iter) += AudioBuffer::getDefaultSamples(); //Increment by a whole channel at once.
}

Atom::Atom(AtomController & parent, int index) :
		m_p(parent),
		m_updateTimer(0),
		m_parameters(parent.getParameters().withId(index)),
		m_sampleRate(0),
		m_sampleRate_f(0.0f),
		m_shouldUpdateParent(false) {
	m_primaryInputs.resize(m_parameters.m_numPrimaryInputs, nullptr);
	m_automationInputs.resize(AUTOMATION_INPUTS, nullptr);
	m_outputs.resize(m_parameters.m_numOutputs, AudioBuffer());
}

Atom::~Atom() {
	// TODO Auto-generated destructor stub
}

void Atom::executeWrapper() {
	if(Synth::getInstance()->getLogManager().shouldDebugEverything()) {
		Synth::getInstance()->getLogManager().addLabel("Voice " + std::to_string(m_parameters.m_id));
	}

	m_sampleRate = Synth::getInstance()->getParameters().m_sampleRate;
	m_sampleRate_f = double(m_sampleRate);
	if (m_parameters.m_id == 0) {
		if (m_updateTimer == 0) {
			m_updateTimer = 5;
			m_shouldUpdateParent = true;
		} else {
			m_updateTimer--;
			m_shouldUpdateParent = false;
		}
	}

	if (m_parameters.m_automationEnabled) {
		m_p.m_automation.calculateAutomation(*this);
	}

	execute();

	if(Synth::getInstance()->getLogManager().shouldDebugEverything()) {
		for(int i = 0; i < m_outputs.size(); i++) {
			Synth::getInstance()->getLogManager().addLabel("Output " + std::to_string(i));
				Synth::getInstance()->getLogManager().writeAudioBuffer(m_outputs[i]);
			Synth::getInstance()->getLogManager().endLabel();
		}
		Synth::getInstance()->getLogManager().endLabel();
	}
}

void Atom::execute() {

}

void Atom::reset() {
	m_shouldUpdateParent = getIndex() == 0;
	if (m_shouldUpdateParent)
		m_p.stopControlAnimation();
}

void Atom::linkInput(int index, AudioBuffer * buffer) {
	if (index < m_parameters.m_numPrimaryInputs) {
		linkPrimaryInput(index, buffer);
	} else {
		linkAutomationInput(index - m_parameters.m_numPrimaryInputs, buffer);
	}
}

void Atom::unlinkInput(int index) {
	if (index < m_parameters.m_numPrimaryInputs) {
		unlinkPrimaryInput(index);
	} else {
		unlinkAutomationInput(index - m_parameters.m_numPrimaryInputs);
	}
}

} /* namespace AtomSynth */
