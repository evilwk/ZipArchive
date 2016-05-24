#ifndef ZIPARCHIVE_H
#define	ZIPARCHIVE_H

#include <cstdio>
#include <string>
#include <vector>
#include <Windows.h>

#include <zipconf.h>
#include "UnicodeConv.h"

struct zip;

#define DIRECTORY_SEPARATOR '/'
#define IS_DIRECTORY(str) (str.length()>0 && str[str.length()-1]==DIRECTORY_SEPARATOR)

class CZipEntry;

class CZipArchive
{
public:

	/*
	 * WRITE ��ӵ�����zip �� ������zip
	 * NEW ������zip �� ɾ������zip����������
	 */
	enum OpenMode { NOT_OPEN, READ_ONLY, WRITE, NEW };

	enum State { ORIGINAL, CURRENT };

	CZipArchive(const std::string &zipPath, bool isUtf8 = false, const std::string &password = "");
	virtual ~CZipArchive(void);

	// ��zip�浵
	bool open(OpenMode mode = READ_ONLY, bool checkConsistency = false);

	// ����zip·��
	std::string getPath(void) const
	{
		return path;
	}

	// ���ش�ģʽ
	OpenMode getMode(void) const
	{
		return mode;
	}

	// �ر�zip�浵
	void close(void);

	// �ر�zip�浵���ع�����
	void discard(void);

	// ɾ���浵
	bool unlink(void);

	// zip�浵�Ƿ��
	bool isOpen(void) const
	{
		return zipHandle != NULL;
	}

	// zip�浵�Ƿ�����޸�
	bool isMutable(void) const
	{
		return isOpen() && mode != NOT_OPEN && mode != READ_ONLY;
	}

	// zip�浵�Ƿ�Ҫ����
	bool isEncrypted(void) const
	{
		return !password.empty();
	}

	// ����zip�浵��ע��
	std::string getComment(State state = CURRENT) const;
	bool setComment(const std::string &comment) const;

	// ɾ��zip�浵��ע��
	bool removeComment(void) const
	{
		return setComment(std::string());
	}

	/**
	 * ����zip�浵�е���Ŀ����(�����ļ���)
	 * �����3����Ŀ����ɾ��һ����Ŀ�����ǻ᷵��3,
	 * ��������һ����Ŀ��CURRENT״̬������4��ORIGINAL������3��
	 *
	 * getEntries���Ի�ȡzip�浵����ʵ��Ŀ
	 */
	zip_int64_t getNbEntries(State state = CURRENT) const;
	zip_int64_t getEntriesCount(State state = CURRENT) const
	{
		return getNbEntries(state);
	}

	// ����������Ŀ
	std::vector<CZipEntry> getEntries(State state = CURRENT) const;

	// �ж���Ŀ�Ƿ����
	bool hasEntry(const std::string &name, bool excludeDirectories = false, bool caseSensitive = true, State state = CURRENT) const;

	// ����������ȡ��Ŀ
	CZipEntry getEntry(const std::string &name, bool excludeDirectories = false, bool caseSensitive = true, State state = CURRENT) const;
	CZipEntry getEntry(zip_int64_t index, State state = CURRENT) const;

	// ������Ŀ��ע��
	std::string getEntryComment(const CZipEntry &entry, State state = CURRENT) const;
	bool setEntryComment(const CZipEntry &entry, const std::string &comment) const;

	// ��ȡ��Ŀ����
	void *readEntry(const CZipEntry &zipEntry, bool asText = false, State state = CURRENT) const;
	void *readEntry(const std::string &zipEntry, bool asText = false, State state = CURRENT) const;
	std::string readString(const std::string &zipEntry, CZipArchive::State state = CZipArchive::CURRENT) const;

	// ����Ŀ����д�뵽�ļ�
	bool writeEntry(const CZipEntry &zipEntry, const std::string &fileName, State state = CURRENT) const;

	// ɾ����Ŀ
	int deleteEntry(const CZipEntry &entry) const;
	int deleteEntry(const std::string &entry) const;

	// ��������Ŀ
	int renameEntry(const CZipEntry &entry, const std::string &newName) const;
	int renameEntry(const std::string &entry, const std::string &newName) const;

	// ����ļ���zip�浵
	bool addFile(const std::string &entryName, const std::string &file) const;

	// ������ݵ�zip�浵
	bool addData(const std::string &entryName, const void *data, unsigned int length, bool freeData = false) const;

	// ���Ŀ¼��Ŀ��zip�浵��entryName������Ŀ¼(��'/'��β)
	bool addEntry(const std::string &entryName) const;

	// UTF8����ת��
#define Utf8ToAscii(str) (isUtf8 ? ConvertUtf8ToMultiBytes(str) : str)
#define AsciiToUtf8(str) (isUtf8 ? ConvertMultiBytesToUtf8(str) : str)
#define DEFAULLT_ENC_FLAG (isUtf8 ? ZIP_FL_ENC_UTF_8 : ZIP_FL_ENC_GUESS)

	// ��ѹzip�浵
	void extract(const std::string &folderName);

	// ���Ŀ¼��zip�浵
	bool addFolder(const std::string &entryName, const std::string &folderName);

	// �ϲ�·��
	std::string concatPath(const std::string &strDir, const std::string &strFile, char slash = '\\');

	// ����Ŀ¼
	void createFolder(const std::string &folderName);

	// ��ȡĿ¼·��
	std::string getFolderPath(const std::string &filePath);

	// time_t ת�� FileTime
	void TimetToFileTime(time_t t, LPFILETIME pft) const;

private:
	std::string path;
	zip *zipHandle;
	OpenMode mode;
	std::string password;
	bool isUtf8;

	// ����ZipEntry
	CZipEntry createEntry(struct zip_stat *stat) const;

	CZipArchive(const CZipArchive &zf);
	CZipArchive &operator=(const CZipArchive &);
};

class CZipEntry
{
	friend class CZipArchive;

public:
	CZipEntry(void) : zipFile(NULL), index(0), time(0), method(-1), size(0), sizeComp(0), crc(0)  {}
	virtual ~CZipEntry(void) {}

	std::string getName(void) const
	{
		return name;
	}

	zip_uint64_t getIndex(void) const
	{
		return index;
	}

	time_t getDate(void) const
	{
		return time;
	}

	int getMethod(void) const
	{
		return method;
	}

	// �����ļ�δѹ���ߴ�
	zip_uint64_t getSize(void) const
	{
		return size;
	}

	// �����ļ�ѹ���ߴ�
	zip_uint64_t getInflatedSize(void) const
	{
		return sizeComp;
	}

	int getCRC(void) const
	{
		return crc;
	}

	bool isDirectory(void) const
	{
		return IS_DIRECTORY(name) || dirFlag;
	}

	bool isFile(void) const
	{
		return !isDirectory();
	}

	bool isNull(void) const
	{
		return zipFile == NULL;
	}

private:
	const CZipArchive *zipFile;
	std::string name;
	zip_uint64_t index;
	time_t time;
	int method;
	zip_uint64_t size;
	zip_uint64_t sizeComp;
	int crc;
	bool dirFlag;

	CZipEntry(
		const CZipArchive *zipFile, 
		const std::string &name, zip_uint64_t index, 
		time_t time, int method, 
		zip_uint64_t size, zip_uint64_t sizeComp, 
		int crc, bool dirFlag
	) : zipFile(zipFile),
		name(name), index(index),
		time(time), method(method),
		size(size), sizeComp(sizeComp),
		crc(crc), dirFlag(dirFlag)
	{

	}
};

#endif

