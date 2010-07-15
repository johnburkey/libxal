/************************************************************************************\
This source file is part of the KS(X) audio library                                  *
For latest info, see http://libatres.sourceforge.net/                                *
**************************************************************************************
Copyright (c) 2010 Kresimir Spes (kreso@cateia.com), Boris Mikic                     *
*                                                                                    *
* This program is free software; you can redistribute it and/or modify it under      *
* the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php   *
\************************************************************************************/
#include <hltypes/hstring.h>
#include "Category.h"
#include "Source.h"
#include "SoundBuffer.h"
#include "StreamSound.h"
#include "AudioManager.h"

#ifndef __APPLE__
	#include <AL/al.h>
#else
	#include <OpenAL/al.h>
#endif

namespace xal
{
/******* CONSTRUCT / DESTRUCT ******************************************/

	Source::Source(SoundBuffer* sound) : sourceId(0), gain(1.0f),
		looping(false), paused(false), fadeTime(0.0f), fadeSpeed(0.0f),
		bound(true), Sound()
	{
		this->sound = sound;
	}

	Source::~Source()
	{
		this->stop();
	}

/******* METHODS *******************************************************/
	
	void Source::update(float k)
	{
		if (this->sourceId == 0)
		{
			return;
		}
		this->sound->setSourceId(this->sourceId);
		this->sound->update(k);
		if (this->isPlaying())
		{
			if (this->isFading())
			{
				this->fadeTime += this->fadeSpeed * k;
				if (this->fadeTime >= 1.0f && this->fadeSpeed > 0.0f)
				{
					alSourcef(this->sourceId, AL_GAIN, this->gain *
						this->sound->getCategory()->getGain() * xal::mgr->getGlobalGain());
					this->fadeTime = 0.0f;
					this->fadeSpeed = 0.0f;
				}
				else if (this->fadeTime <= 0.0f && this->fadeSpeed < 0.0f)
				{
					this->paused ? this->pause() : this->stop();
					this->fadeTime = 0.0f;
					this->fadeSpeed = 0.0f;
				}
				else
				{
					alSourcef(this->sourceId, AL_GAIN, this->fadeTime * this->gain *
						this->sound->getCategory()->getGain() * xal::mgr->getGlobalGain());
				}
			}
		}
		if (!this->isPlaying() && !this->isPaused())
		{
			this->unbind();
		}
	}

	Sound* Source::play(float fadeTime, bool looping)
	{
		if (this->sourceId == 0)
		{
			this->sourceId = xal::mgr->allocateSourceId();
			if (this->sourceId == 0)
			{
				return NULL;
			}
		}
		if (!this->isPaused())
		{
			//2DO - get remembered sample offset and set source to buffer position
		}
		if (!this->paused)
		{
			this->looping = looping;
		}
		if (this->sound->getCategory()->isStreamed())
		{
			if (!this->isPaused())
			{
				this->sound->setSourceId(this->sourceId);
				((StreamSound*)this->sound)->queueBuffers();
				alSourcei(this->sourceId, AL_LOOPING, false);
			}
		}
		else if (!this->isPaused())
		{
			alSourcei(this->sourceId, AL_BUFFER, this->getBuffer());
			alSourcei(this->sourceId, AL_LOOPING, this->looping);
		}
		bool alreadyFading = this->isFading();
		if (fadeTime > 0)
		{
			this->fadeSpeed = 1.0f / fadeTime;
		}
		else
		{
			this->fadeTime = 1.0f;
			this->fadeSpeed = 0.0f;
		}
		alSourcef(this->sourceId, AL_GAIN, this->fadeTime * this->gain *
			this->sound->getCategory()->getGain() * xal::mgr->getGlobalGain());
		if (!alreadyFading)
		{
			alSourcePlay(this->sourceId);
		}
		this->paused = false;
		return this;
	}

	void Source::stop(float fadeTime)
	{
		if (this->sourceId == 0)
		{
			return;
		}
		if (fadeTime > 0)
		{
			this->fadeSpeed = -1.0f / fadeTime;
		}
		else
		{
			this->fadeTime = 0.0f;
			this->fadeSpeed = 0.0f;
			alSourceStop(this->sourceId);
			this->unbind();
		}
		this->paused = false;
	}

	void Source::pause(float fadeTime)
	{
		if (this->sourceId == 0)
		{
			return;
		}
		if (fadeTime > 0)
		{
			this->fadeSpeed = -1.0f / fadeTime;
		}
		else
		{
			this->fadeTime = 0.0f;
			this->fadeSpeed = 0.0f;
			alSourcePause(this->sourceId);
			//2DO - uncomment, remember sample offset
			//this->unbind();
		}
		this->paused = true;
	}

	void Source::unbind()
	{
		if (!this->isLocked())
		{
			this->sourceId = 0;
			this->bound = false;
		}
	}
	
/******* PROPERTIES ****************************************************/

	float Source::getSampleOffset()
	{
		//2DO - change implementation
		if (this->sourceId == 0)
		{
			return 0.0f;
		}
		float value;
		alGetSourcef(this->sourceId, AL_SEC_OFFSET, &value);
		return value;
	}

	void Source::setGain(float gain)
	{
		this->gain = gain;
		if (this->sourceId != 0)
		{
			alSourcef(this->sourceId, AL_GAIN, this->gain *
				this->sound->getCategory()->getGain() * xal::mgr->getGlobalGain());
		}
	}

	unsigned int Source::getBuffer()
	{
		return this->sound->getBuffer();
	}
	
	bool Source::isPlaying()
	{
		if (this->sourceId == 0)
		{
			return false;
		}
		if (this->sound->getCategory()->isStreamed())
		{
			return (!this->isPaused());
		}
		int state;
		alGetSourcei(this->sourceId, AL_SOURCE_STATE, &state);
		return (state == AL_PLAYING);
	}

	bool Source::isPaused()
	{
		return (this->paused && !this->isFading());
	}
	
	bool Source::isFading()
	{
		return (this->fadeSpeed != 0);
	}

	bool Source::isFadingIn()
	{
		return (this->fadeSpeed < 0);
	}

	bool Source::isFadingOut()
	{
		return (this->fadeSpeed < 0);
	}

}