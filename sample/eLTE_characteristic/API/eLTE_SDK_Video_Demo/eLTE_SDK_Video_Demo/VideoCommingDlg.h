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

#pragma once


// VideoCommingDlg dialog

class CDConsoleDlg;

class VideoCommingDlg : public CDialogEx
{
	DECLARE_DYNAMIC(VideoCommingDlg)

public:
	VideoCommingDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~VideoCommingDlg();

// Dialog Data
	enum { IDD = IDD_DIALOG_VIDEO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
public:
	void SetResId(CString strResId) {m_strResId = strResId;};
	void SetDConsoleDlg(CDConsoleDlg* pDlg);
private:
	CString m_strResId;
	CDConsoleDlg* m_dcDlg;
};