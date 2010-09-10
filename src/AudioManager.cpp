/************************************************************************************\
This source file is part of the KS(X) audio library                                  *
For latest info, see http://code.google.com/p/libxal/                                *
**************************************************************************************
Copyright (c) 2010 Kresimir Spes (kreso@cateia.com), Boris Mikic                     *
*                                                                                    *
* This program is free software; you can redistribute it and/or modify it under      *
* the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php   *
\************************************************************************************/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include <hltypes/hstring.h>
#include <hltypes/util.h>

#ifndef __APPLE__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

#include "AudioManager.h"
#include "Category.h"
#include "Mutex.h"
#include "SimpleSound.h"
#include "SoundBuffer.h"
#include "Source.h"
#include "StreamSound.h"
#include "Thread.h"
#include "Util.h"

namespace xal
{
/******* GLOBAL ********************************************************/
	
	AudioManager* mgr;

	void init(chstr deviceName, bool threaded, float updateTime)
	{
		mgr = new AudioManager();
		mgr->init(deviceName, threaded, updateTime);
	}
	
	void destroy()
	{
		delete mgr;
	}
	
	void xal_writelog(chstr text)
	{
		printf("%s\n", text.c_str());
	}
	
	void (*gLogFunction)(chstr) = xal_writelog;
	ALCdevice* gDevice = NULL;
	ALCcontext* gContext = NULL;

	void setLogFunction(void (*function)(chstr))
	{
		gLogFunction = function;
	}
	
/******* CONSTRUCT / DESTRUCT ******************************************/

	AudioManager::AudioManager() : deviceName(""), updateTime(0.01f),
		sources(harray<Source*>()), gain(1.0f),
		categories(std::map<hstr, Category*>()),
		sounds(std::map<hstr, SoundBuffer*>()), thread(NULL), mutex(NULL)
	{
	}
	
	void AudioManager::init(chstr deviceName, bool threaded, float updateTime)
	{
		this->logMessage("Initializing XAL");
		if (deviceName == "nosound")
		{
			this->deviceName = "nosound";
			this->logMessage("- Audio is disabled");
			return;
		}
		this->logMessage("Initializing OpenAL");
		ALCdevice* currentDevice = alcOpenDevice(deviceName.c_str());
		if (alcGetError(currentDevice) != ALC_NO_ERROR)
		{
			return;
		}
		this->deviceName = alcGetString(currentDevice, ALC_DEVICE_SPECIFIER);
		this->logMessage("Audio device: " + this->deviceName);
		ALCcontext* currentContext = alcCreateContext(currentDevice, NULL);
		if (alcGetError(currentDevice) != ALC_NO_ERROR)
		{
			return;
		}
		alcMakeContextCurrent(currentContext);
		if (alcGetError(currentDevice) != ALC_NO_ERROR)
		{
			return;
		}
		alGenSources(XAL_MAX_SOURCES, this->sourceIds);
		gDevice = currentDevice;
		gContext = currentContext;
		this->deviceName = deviceName;
		if (threaded)
		{
			this->logMessage("Starting Thread Management");
			this->updateTime = updateTime;
			this->mutex = new Mutex();
			this->thread = new Thread();
			this->thread->start();
		}
	}

	AudioManager::~AudioManager()
	{
		if (this->mutex != NULL)
		{
			this->mutex->lock();
			this->thread->join();
			this->mutex->unlock();
			delete this->thread;
			delete this->mutex;
		}
		foreach_in_map (SoundBuffer*, it, this->sounds)
		{
			delete it->second;
		}
		foreach_in_map (Category*, it, this->categories)
		{
			delete it->second;
		}
		this->logMessage("Destroying OpenAL");
		if (gDevice)
		{
			while (this->sources.size() > 0)
			{
				this->sources[0]->unlock();
				this->sources[0]->stop();
				this->destroySource(this->sources[0]);
			}
			alDeleteSources(XAL_MAX_SOURCES, this->sourceIds);
			alcMakeContextCurrent(NULL);
			alcDestroyContext(gContext);
			alcCloseDevice(gDevice);
		}
	}
	
/******* PROPERTIES ****************************************************/

	void AudioManager::logMessage(chstr message, chstr prefix)
	{
		gLogFunction(prefix + message);
	}
	
	bool AudioManager::isEnabled()
	{
		return (gDevice != NULL);
	}

	void AudioManager::update()
	{
		this->update(this->updateTime);
	}
	
	void AudioManager::update(float k)
	{
		if (this->isEnabled())
		{
			foreach (Source*, it, this->sources)
			{
				(*it)->update(k);
			}
			harray<Source*> sources(this->sources);
			foreach (Source*, it, sources)
			{
				if (!(*it)->isBound())
				{
					(*it)->getSound()->unbindSource(*it);
					this->destroySource(*it);
				}
			}
		}
	}

	unsigned int AudioManager::allocateSourceId()
	{
		harray<unsigned int> allocated;
		unsigned int id = 0;
		foreach (Source*, it, this->sources)
		{
			id = (*it)->getSourceId();
			if (id != 0)
			{
				allocated += id;
			}
		}
		harray<unsigned int> unallocated(this->sourceIds, XAL_MAX_SOURCES);
		unallocated -= allocated;
		if (unallocated.size() > 0)
		{
			return unallocated[0];
		}
		this->logMessage("Audio Manager: Unable to allocate audio source!");
		return 0;
	}

	Source* AudioManager::createSource(SoundBuffer* sound, unsigned int sourceId)
	{
		Source* source = new Source(sound, sourceId);
		this->sources += source;
		return source;
	}
	
	void AudioManager::destroySource(Source* source)
	{
		this->sources -= source;
		delete source;
	}

	Sound* AudioManager::getSound(chstr name)
	{
		if (this->sounds.find(name) == this->sounds.end())
		{
			return NULL;
		}
		return this->sounds[name];
	}
	
	Sound* AudioManager::createSound(chstr filename, chstr categoryName, chstr prefix)
	{
		Category* category = this->getCategoryByName(categoryName);
		SoundBuffer* sound;
		if (category->isStreamed())
		{
			sound = new StreamSound(filename, categoryName, prefix);
		}
		else
		{
			sound = new SimpleSound(filename, categoryName, prefix);
		}
		if (!sound->load())
		{
			return NULL;
		}
		this->sounds[sound->getName()] = sound;
		return sound;
	}

	harray<hstr> AudioManager::createSoundsFromPath(chstr path, chstr prefix)
	{
		harray<hstr> result;
		hstr category;
		harray<hstr> dirs = getPathDirectories(path);
		foreach (hstr, it, dirs)
		{
			category = (*it).rsplit("/").pop_back();
			result += this->createSoundsFromPath(hsprintf("%s/%s", path.c_str(), (*it).c_str()), category, prefix);
		}
		return result;
	}

	harray<hstr> AudioManager::createSoundsFromPath(chstr path, chstr category, chstr prefix)
	{
		this->createCategory(category);
		harray<hstr> result;
		harray<hstr> files = getPathFilesRecursive(path);
		SoundBuffer* sound;
		foreach (hstr, it, files)
		{
			sound = (SoundBuffer*)this->createSound(hsprintf("%s/%s", path.c_str(), (*it).c_str()), category, prefix);
			if (sound != NULL)
			{
				result += sound->getName();
			}
		}
		return result;
	}

	void AudioManager::destroySound(SoundBuffer* sound)
	{
		foreach_in_map (SoundBuffer*, it, this->sounds)
		{
			if (it->second == sound)
			{
				this->sounds.erase(it);
				delete it->second;
				break;
			}
		}
	}
	
	void AudioManager::destroySoundsWithPrefix(chstr prefix)
	{
		harray<hstr> deleteList;
		foreach_in_map (SoundBuffer*, it, this->sounds)
		{
			if (it->first.starts_with(prefix))
			{
				delete it->second;
				deleteList.push_back(it->first);
			}
		}
		foreach (hstr, it, deleteList)
		{
			this->sounds.erase(*it);
		}
	}

	void AudioManager::createCategory(chstr name, bool streamed)
	{
		if (this->categories.find(name) == this->categories.end())
		{
			this->categories[name] = new Category(name, streamed);
		}
	}

	void AudioManager::setGlobalGain(float value)
	{
		this->gain = gain;
		foreach (Source*, it, this->sources)
		{
			alSourcef((*it)->getSourceId(), AL_GAIN, (*it)->getSound()->getCategory()->getGain() *
				(*it)->getGain() * this->gain);
		}
	}

	Category* AudioManager::getCategoryByName(chstr name)
	{
		if (this->categories.find(name) == this->categories.end())
		{
			throw ("Audio Manager: Category '" + name + "' does not exist!").c_str();
		}
		return this->categories[name];
	}

	float AudioManager::getCategoryGain(chstr name)
	{
		if (this->categories.find(name) == this->categories.end())
		{
			throw ("Audio Manager: Category '" + name + "' does not exist!").c_str();
		}
		return this->categories[name]->getGain();
	}

	void AudioManager::setCategoryGain(chstr name, float gain)
	{
		this->getCategoryByName(name)->setGain(gain);
		foreach (Source*, it, this->sources)
		{
			if ((*it)->getSound()->getCategory()->getName() == name)
			{
				alSourcef((*it)->getSourceId(), AL_GAIN, gain * (*it)->getGain() * this->gain);
			}
		}
	}

	void AudioManager::stopAll(float fadeTime)
	{
		harray<Source*> sources(this->sources);
		foreach (Source*, it, sources)
		{
			(*it)->unlock();
			(*it)->stop(fadeTime);
		}
	}
	
	void AudioManager::stopCategory(chstr category, float fadeTime)
	{
		harray<Source*> sources(this->sources);
		foreach (Source*, it, sources)
		{
			if ((*it)->getSound()->getCategory()->getName() == category)
			{
				(*it)->unlock();
				(*it)->stop(fadeTime);
			}
		}
	}
	
}