/*Copyright 2015 Huawei Technologies Co., Ltd. All rights reserved.
eSDK is licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
		http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "StdAfx.h"
#include "AudioPlayer.h"
#include "eLTE_Log.h"


AudioPlayer::AudioPlayer(void)
	: m_hWaveOut(NULL)
	, m_uIdxToWrite(0)
	, m_uIdxToRead(0)
	, m_dwVolume(0)
	, m_orgVolume(0)
{
	memset(&m_waveformat, 0, sizeof(m_waveformat));
	memset(m_WaveOutBuf, 0, sizeof(WAVE_OUT_BUF)*WAVE_RING_SZ);
}


AudioPlayer::~AudioPlayer(void)
{
	if (NULL != m_hWaveOut)
	{
		(void)waveOutClose(m_hWaveOut);
		m_hWaveOut = NULL;
	}
	m_orgVolume = 0;
}


// ��ʼ������
int AudioPlayer::WaveOutInit()
{
	LOG_TRACE();

	// ��ʼ��opencore-amr��Ƶת��
	int iRet = m_OpencoreAmrnb.Init();
	if (eLTE_ERR_SUCCESS != iRet)
	{
		LOG_RUN_ERROR("OpencoreAmrnb init failed.");
		return iRet;
	}

	if (NULL == m_hWaveOut)
	{
		// ������Ƶ����
		memset(&m_waveformat, 0, sizeof(m_waveformat));
		m_waveformat.wFormatTag = WAVE_FORMAT_PCM;	// ��Ƶ��ʽ
		m_waveformat.nChannels = 1;					// 1��������2������
		m_waveformat.nSamplesPerSec = 8000;			// ����Ƶ��8.0 KHz
		m_waveformat.nAvgBytesPerSec = 16000;		// ƽ�����ݴ�����,PCM nAvgBytesPerSec = nSamplesPerSec
		m_waveformat.nBlockAlign = 2;				// ���ֽ�Ϊ��λ���ÿ����, (nChannels*wBitsPerSample)/8
		m_waveformat.wBitsPerSample = 16;			// ������λ��
		m_waveformat.cbSize = 0;					// ������Ϣ�Ĵ�С, PCM���Դ�ֵ

		// ����Ƶ�豸
		MMRESULT mRet = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &m_waveformat, (DWORD_PTR)WaveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);//lint !e620
		if (MMSYSERR_NOERROR != mRet)  
		{
			LOG_RUN_ERROR("waveOutOpen failed. (%d)", mRet);
			return eLTE_ERR_AUDIO_OPEN;  
		}

		iRet = (int)waveOutGetVolume(m_hWaveOut, &m_orgVolume);
		if (eLTE_ERR_SUCCESS != iRet)
		{
			LOG_RUN_ERROR("waveOutGetVolume failed.");
			return iRet;
		}
		// ��������
		iRet = WaveOutSetVolume(m_dwVolume);
		if (eLTE_ERR_SUCCESS != iRet)
		{
			LOG_RUN_ERROR("WaveOutSetVolume failed.");
			return iRet;
		}
	}

	return eLTE_ERR_SUCCESS;
}

// �˳�����
int AudioPlayer::WaveOutExit()
{
	LOG_TRACE();

	// �ر���Ƶ�豸
	if (NULL != m_hWaveOut)
	{
		MMRESULT mRet = waveOutReset(m_hWaveOut);
		if (MMSYSERR_NOERROR != mRet)  
		{
			LOG_RUN_ERROR("waveOutReset failed. (%d)", mRet);
			return eLTE_ERR_AUDIO_RESET;  
		}

		// �ȴ����Ž���
		while (m_uIdxToRead != m_uIdxToWrite)
		{
			mRet = waveOutUnprepareHeader(m_hWaveOut, &m_WaveOutBuf[m_uIdxToRead].Hdr, sizeof(WAVEHDR));
			if (MMSYSERR_NOERROR != mRet)  
			{
				LOG_RUN_ERROR("waveOutUnprepareHeader failed. (%d)", mRet);
				return eLTE_ERR_AUDIO_UNPREPARE;  
			}

			m_WaveOutBuf[m_uIdxToRead].bInUse = FALSE;
			m_uIdxToRead++;
			m_uIdxToRead = m_uIdxToRead % WAVE_RING_SZ;
		}

		//���ԭʼϵͳ����
		if(0 == m_orgVolume)
		{
			m_orgVolume = 100;
			if(!WaveOutGetVolume(m_orgVolume))
			{
				return eLTE_ERR_FAILED;
			}
		}

		mRet = waveOutSetVolume(m_hWaveOut, m_orgVolume);
		if (eLTE_ERR_SUCCESS != mRet)
		{
			LOG_RUN_ERROR("waveOutSetVolume failed.");
			return (int)mRet;
		}

		mRet = waveOutClose(m_hWaveOut);
		if (MMSYSERR_NOERROR != mRet)  
		{
			LOG_RUN_ERROR("waveOutClose failed. (%d)", mRet);
			return eLTE_ERR_AUDIO_CLOSE;
		}
		m_hWaveOut = NULL;
	}

	// �˳�opencore-amr��Ƶת��
	int iRet = m_OpencoreAmrnb.Exit();
	if (eLTE_ERR_SUCCESS != iRet)
	{
		LOG_RUN_ERROR("OpencoreAmrnb Exit failed. (%d)", iRet);
	}

	return iRet;
}

// ����PCM
int AudioPlayer::WaveOutPlay(char* pAmr, unsigned int len)
{
	// ����ж�
	if (NULL == pAmr)
	{
		LOG_RUN_ERROR("pAmr is null.");
		return eLTE_ERR_INVALID_PARAM;
	}
	if (AUDIO_FRAME_DATA_SZ < len)
	{
		LOG_RUN_ERROR("len is overlong.");
		return eLTE_ERR_INVALID_PARAM;
	}

	// ״̬У��
	if (NULL == m_hWaveOut)
	{
		LOG_RUN_ERROR("Please WaveOutInit first.");
		return eLTE_ERR_AUDIO_OPEN;
	}

	// ��������
	if (m_WaveOutBuf[m_uIdxToWrite].bInUse)
	{
		LOG_RUN_ERROR("RingBuffer is full.");
		return eLTE_ERR_AUDIO_OUT_BUFFER;
	}

	// AMR ת��Ϊ WAV
	char* pWav = (char*)m_OpencoreAmrnb.DecodeAmrToWav((unsigned char*)pAmr);
	if (NULL == pWav)
	{
		LOG_RUN_ERROR("DecodeAmrToWav failed.");
		return eLTE_ERR_AUDIO_DECODE;
	}

	// ����
	WAVE_OUT_BUF* pWaveOutBuf = &m_WaveOutBuf[m_uIdxToWrite];
	if (pWaveOutBuf->Hdr.dwFlags & WHDR_PREPARED)
	{
		(void)waveOutUnprepareHeader(m_hWaveOut, &pWaveOutBuf->Hdr, sizeof(WAVEHDR));
	}

	pWaveOutBuf->Hdr.lpData = pWaveOutBuf->WaveData;
	pWaveOutBuf->Hdr.dwBufferLength = WAV_BUFFER_SIZE;
	pWaveOutBuf->Hdr.dwBytesRecorded = 0;
	pWaveOutBuf->Hdr.dwUser =  0;
	pWaveOutBuf->Hdr.dwFlags = 0; 
	pWaveOutBuf->Hdr.dwLoops = 0; 
	pWaveOutBuf->Hdr.lpNext = NULL;
	pWaveOutBuf->Hdr.reserved = 0;
	memcpy(pWaveOutBuf->Hdr.lpData, pWav, WAV_BUFFER_SIZE);

	int iRet = eLTE_ERR_SUCCESS;
	MMRESULT mRet = waveOutPrepareHeader(m_hWaveOut, &pWaveOutBuf->Hdr, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != mRet)
	{
		LOG_RUN_ERROR("waveOutPrepareHeader failed. (%d)", mRet);
		iRet = eLTE_ERR_AUDIO_PREPARE;
	}

	pWaveOutBuf->bInUse = TRUE;
	mRet = waveOutWrite(m_hWaveOut, &pWaveOutBuf->Hdr, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != mRet)
	{              
		LOG_RUN_ERROR("waveOutWrite failed. (%d)", mRet);
		iRet = eLTE_ERR_AUDIO_WRITE;
	}

	m_uIdxToWrite++;
	m_uIdxToWrite = m_uIdxToWrite % WAVE_RING_SZ;

	return iRet;
}

// ���ò�������
int AudioPlayer::WaveOutSetVolume(DWORD dwVolume)
{
	LOG_TRACE();
	INFO_PARAM1(dwVolume);
	if (NULL == m_hWaveOut)
	{
		LOG_RUN_ERROR("Please WaveOutInit first.");
		return eLTE_ERR_AUDIO_OPEN;
	}

	dwVolume = (ULONG)(((float)0xFFFF / 100) * (float)dwVolume);
	dwVolume = (dwVolume << 16) + dwVolume;

	MMRESULT mRet = waveOutSetVolume(m_hWaveOut, dwVolume);
	if (MMSYSERR_NOERROR != mRet)
	{              
		LOG_RUN_ERROR("waveOutSetVolume failed. (%d)", mRet);
		return eLTE_ERR_AUDIO_SET_VOLUME;
	}

	m_dwVolume = dwVolume;
	return eLTE_ERR_SUCCESS;
}

// ��ȡ��������
int AudioPlayer::WaveOutGetVolume(DWORD& dwVolume)
{
	LOG_TRACE();

	if (NULL == m_hWaveOut)
	{
		LOG_RUN_ERROR("Please WaveOutInit first.");
		return eLTE_ERR_AUDIO_OPEN;
	}

	MMRESULT mRet = waveOutGetVolume(m_hWaveOut, &dwVolume);
	if (MMSYSERR_NOERROR != mRet)
	{              
		LOG_RUN_ERROR("waveOutGetVolume failed. (%d)", mRet);
		return eLTE_ERR_AUDIO_GET_VOLUME;
	}

	dwVolume = (dwVolume >> 16) * 100 / 0xFFFF;

	INFO_PARAM1(dwVolume);
	m_dwVolume = dwVolume;
	return eLTE_ERR_SUCCESS;
}

// ��ʼ������
int AudioPlayer::WaveOutInitVolume(DWORD dwVolume)
{
	LOG_TRACE();
	INFO_PARAM1(dwVolume);

	m_dwVolume = dwVolume;

	return eLTE_ERR_SUCCESS;
}

// waveOut�ص�����
void CALLBACK AudioPlayer::WaveOutProc(HWAVEOUT /*hwo*/, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR /*dwParam1*/, DWORD_PTR /*dwParam2*/)
{
	if (WOM_DONE != uMsg)
	{
		return;
	}

	AudioPlayer* pAudioPlayer = (AudioPlayer*)dwInstance;
	if (NULL == pAudioPlayer)
	{
		LOG_RUN_ERROR("pAudioPlayer is null.");
		return;
	}

	WAVE_OUT_BUF* pWaveOutBuf = &pAudioPlayer->m_WaveOutBuf[pAudioPlayer->m_uIdxToRead];
	if (!(pWaveOutBuf->Hdr.dwFlags & WHDR_DONE))
	{
		LOG_RUN_ERROR("WAVEHDR.dwFlags is not WHDR_DONE.");
	}

	pWaveOutBuf->bInUse = FALSE;
	pAudioPlayer->m_uIdxToRead++;
	pAudioPlayer->m_uIdxToRead = pAudioPlayer->m_uIdxToRead % WAVE_RING_SZ;
}