/// @file
/// @author  Boris Mikic
/// @author  Ivan Vucica
/// @version 2.1
/// 
/// @section LICENSE
/// 
/// This program is free software; you can redistribute it and/or modify it under
/// the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php
/// 
/// @section DESCRIPTION
/// 
/// Represents an implementation of the Player for CoreAudio.

#if HAVE_COREAUDIO
#ifndef XAL_COREAUDIO_PLAYER_H
#define XAL_COREAUDIO_PLAYER_H

#include "Player.h"
#include "xalExport.h"

namespace xal
{
	class Buffer;
	class Sound;

	class xalExport CoreAudio_Player : public Player
	{
	public:
		CoreAudio_Player(Sound* sound, Buffer* buffer);
		~CoreAudio_Player();

		void mixAudio(unsigned char* stream, int length, bool first);

	protected:
		bool playing;
		int position;
		float currentGain;
		unsigned char circleBuffer[STREAM_BUFFER];
		int readPosition;
		int writePosition;

		void _update(float k);

		bool _systemIsPlaying() { return this->playing; }
		float _systemGetOffset();
		void _systemSetOffset(float value);
		bool _systemPreparePlay();
		void _systemPrepareBuffer();
		void _systemUpdateGain();
		void _systemUpdateFadeGain();
		void _systemPlay();
		void _systemStop();
		void _systemUpdateStream();

		int _fillBuffer(int size);
		void _getData(int size, unsigned char** data1, int* size1, unsigned char** data2, int* size2);

	};

}
#endif
#endif