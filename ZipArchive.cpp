#include <iosfwd>
#include <zip.h>
#include "ZipArchive.h"

using namespace std;

CZipArchive::CZipArchive(const std::string &zipPath, bool isUtf8 /*= false*/, const std::string &password /*= ""*/) : path(zipPath), isUtf8(isUtf8),
zipHandle(NULL), mode(NOT_OPEN), password(password)
{

}

CZipArchive::~CZipArchive(void)
{
	close();
}

bool CZipArchive::open(OpenMode mode, bool checkConsistency)
{
	int zipFlag = 0;
	if (mode == READ_ONLY)
		zipFlag = 0;
	else if (mode == WRITE)
		zipFlag = ZIP_CREATE;
	else if (mode == NEW)
		zipFlag = ZIP_CREATE | ZIP_TRUNCATE;
	else
		return false;

	if (checkConsistency)
		zipFlag = zipFlag | ZIP_CHECKCONS;

	int errorFlag = 0;
	zipHandle = zip_open(path.c_str(), zipFlag, &errorFlag);

	if (errorFlag != ZIP_ER_OK)
	{
		zipHandle = NULL;
		return false;
	}

	if (zipHandle != NULL)
	{
		if (isEncrypted())
		{
			int result = zip_set_default_password(zipHandle, password.c_str());
			if (result != 0)
			{
				close();
				return false;
			}
		}

		this->mode = mode;
		return true;
	}

	return false;
}

void CZipArchive::close(void)
{
	if (zipHandle)
	{
		zip_close(zipHandle);
		zipHandle = NULL;
		mode = NOT_OPEN;
	}
}

void CZipArchive::discard(void)
{
	if (zipHandle)
	{
		zip_discard(zipHandle);
		zipHandle = NULL;
		mode = NOT_OPEN;
	}
}

bool CZipArchive::unlink(void)
{
	if (isOpen())
		discard();

	int result = remove(path.c_str());
	return result == 0;
}

string CZipArchive::getComment(State state) const
{
	if (!isOpen())
		return string();

	int flag = DEFAULLT_ENC_FLAG;
	if (state == ORIGINAL)
		flag = flag | ZIP_FL_UNCHANGED;

	int length = 0;
	const char *comment = zip_get_archive_comment(zipHandle, &length, flag);
	if (comment == NULL)
		return string();
	
	return Utf8ToAscii(string(comment, length));
}

bool CZipArchive::setComment(const string &comment) const
{
	if (!isOpen())
		return false;
	
	string realComment = AsciiToUtf8(comment);
	int size = realComment.size();
	const char *data = realComment.c_str();
	int result = zip_set_archive_comment(zipHandle, data, size);
	return result == 0;
}

zip_int64_t CZipArchive::getNbEntries(State state) const
{
	if (!isOpen())
		return -1;

	int flag = state == ORIGINAL ? ZIP_FL_UNCHANGED : 0;
	return zip_get_num_entries(zipHandle, flag);
}

CZipEntry CZipArchive::createEntry(struct zip_stat *stat) const
{
	string name(stat->name);
	zip_uint64_t index = stat->index;
	zip_uint64_t size = stat->size;
	int method = stat->comp_method;
	zip_uint64_t sizeComp = stat->comp_size;
	int crc = stat->crc;
	time_t time = stat->mtime;
	
	zip_uint8_t system;
	zip_uint32_t attributes;
	zip_file_get_external_attributes(zipHandle, index, stat->flags, &system, &attributes);
	bool dirFlag = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	return CZipEntry(this, Utf8ToAscii(name), index, time, method, size, sizeComp, crc, dirFlag);
}

vector<CZipEntry> CZipArchive::getEntries(State state) const
{
	if (!isOpen())
		return vector<CZipEntry>();

	struct zip_stat stat;
	zip_stat_init(&stat);  

	vector<CZipEntry> entries;
	// 直接返回zip中原生编码的文件名
	int flag = (state == ORIGINAL) ? ZIP_FL_UNCHANGED : 0;
	zip_int64_t nbEntries = getNbEntries(state);
	for (zip_int64_t i = 0 ; i < nbEntries ; ++i)
	{
		int result = zip_stat_index(zipHandle, i, flag, &stat);
		if (result == 0)
		{
			CZipEntry entry = createEntry(&stat);
			entries.push_back(entry);
		}
	}
	return entries;
}

bool CZipArchive::hasEntry(const string &name, bool excludeDirectories, bool caseSensitive, State state) const
{
	CZipEntry entry = getEntry(name, excludeDirectories, caseSensitive, state);
	if (entry.isNull())
		return false;
	return true;
}

CZipEntry CZipArchive::getEntry(const string &name, bool excludeDirectories, bool caseSensitive, State state) const
{
	string realName = Utf8ToAscii(name);
	if (isOpen())
	{
		int flags = DEFAULLT_ENC_FLAG;
		if (excludeDirectories)
			flags = flags | ZIP_FL_NODIR;

		if (!caseSensitive)
			flags = flags | ZIP_FL_NOCASE;

		if (state == ORIGINAL)
			flags = flags | ZIP_FL_UNCHANGED;

		zip_int64_t index = zip_name_locate(zipHandle, realName.c_str(), flags);
		if (index >= 0)
			return getEntry(index);
	}
	return CZipEntry();
}

CZipEntry CZipArchive::getEntry(zip_int64_t index, State state) const
{
	if (isOpen())
	{
		struct zip_stat stat;
		zip_stat_init(&stat);
		int flag = state == ORIGINAL ? ZIP_FL_UNCHANGED : 0;
		int result = zip_stat_index(zipHandle, index, flag, &stat);
		if (result == 0)
			return createEntry(&stat);
	}
	return CZipEntry();
}

string CZipArchive::getEntryComment(const CZipEntry &entry, State state) const
{
	if (!isOpen())
		return string();

	if (entry.zipFile != this)
		return string();

	int flag = DEFAULLT_ENC_FLAG;
	if (state == ORIGINAL)
		flag = flag | ZIP_FL_UNCHANGED;

	unsigned int clen;
	const char *com = zip_file_get_comment(zipHandle, entry.getIndex(), &clen, flag);
	string comment = com == NULL ? string() : string(com, clen);
	return Utf8ToAscii(comment);
}

bool CZipArchive::setEntryComment(const CZipEntry &entry, const string &comment) const
{
	if (!isOpen())
		return false;

	if (entry.zipFile != this)
		return false;
	
	string realComment = AsciiToUtf8(comment);
	bool result = zip_file_set_comment(zipHandle, entry.getIndex(), realComment.c_str(), realComment.size(), DEFAULLT_ENC_FLAG);
	return result == 0;
}

void *CZipArchive::readEntry(const CZipEntry &zipEntry, bool asText, State state) const
{
	if (zipEntry.isNull())
		return NULL;

	if (!isOpen())
		return NULL;

	if (zipEntry.zipFile != this)
		return NULL;

	int flag = state == ORIGINAL ? ZIP_FL_UNCHANGED : 0;
	struct zip_file *zipFile = zip_fopen_index(zipHandle, zipEntry.getIndex(), flag);
	if (zipFile)
	{
		zip_uint64_t size = zipEntry.getSize();
		zip_int64_t isize = (zip_int64_t)size; //there will be a warning here, but unavoidable...

		char *data = new char[isize + (asText ? 1 : 0)];
		if (!data)  //allocation error
		{
			zip_fclose(zipFile);
			return NULL;
		}

		zip_int64_t result = zip_fread(zipFile, data, size);
		zip_fclose(zipFile);

		//avoid buffer copy
		if (asText)
			data[isize] = '\0';

		if (result == isize)
			return data;
		else     //unexpected number of bytes read => crash ?
			delete[] data;
	}

	return NULL;
}

void *CZipArchive::readEntry(const string &zipEntry, bool asText, State state) const
{
	return readEntry(getEntry(zipEntry), asText, state);
}

std::string CZipArchive::readString(const std::string &zipEntry, State state /*= CURRENT*/) const
{
	CZipEntry entry = getEntry(zipEntry);
	char *content = (char *)readEntry(entry, true, state);
	if (content == NULL)
		return string();

	string str(content, entry.getSize());
	delete[] content;
	return str;
}

bool CZipArchive::writeEntry(const CZipEntry &zipEntry, const string &fileName, State state /*= CURRENT*/) const
{
	if (zipEntry.isNull())
		return false;

	if (!isOpen())
		return false;

	if (zipEntry.zipFile != this)
		return false;

	int flag = state == ORIGINAL ? ZIP_FL_UNCHANGED : 0;
	struct zip_file *zipFile = zip_fopen_index(zipHandle, zipEntry.getIndex(), flag);
	if (!zipFile)
		return false;

	zip_uint64_t size = zipEntry.getSize();
	zip_int64_t isize = (zip_int64_t)size; //there will be a warning here, but unavoidable...
	
	// 创建文件
	HANDLE hFile = CreateFileA(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		zip_fclose(zipFile);
		return false;
	}
	
	// 读取内容
	char data[4096];
	zip_int64_t readCount;
	while ((readCount = zip_fread(zipFile, data, sizeof(data))) > 0)
	{
		DWORD dwWritten = 0;
		WriteFile(hFile, data, (DWORD)readCount, &dwWritten, NULL);
	}
	
	// 设置文件时间
	FILETIME ftUTC;
	TimetToFileTime(zipEntry.getDate(), &ftUTC);
	SetFileTime(hFile, &ftUTC, &ftUTC, &ftUTC);

	CloseHandle(hFile);
	zip_fclose(zipFile);

	return true;
}

int CZipArchive::deleteEntry(const CZipEntry &entry) const
{
	if (!isOpen())
		return -3;

	if (entry.zipFile != this)
		return -3;

	if (mode == READ_ONLY)
		return -1;    //deletion not allowed

	if (entry.isFile())
	{
		int result = zip_delete(zipHandle, entry.getIndex());
		if (result == 0)
			return 1;

		return -2; //unable to delete the entry
	}
	else
	{
		int counter = 0;
		vector<CZipEntry> allEntries = getEntries();
		vector<CZipEntry>::const_iterator eit;
		for (eit = allEntries.begin() ; eit != allEntries.end() ; ++eit)
		{
			CZipEntry ze = *eit;
			int startPosition = ze.getName().find(entry.getName());
			if (startPosition == 0)
			{
				int result = zip_delete(zipHandle, ze.getIndex());
				if (result == 0)
					++counter;
				else
					return -2;    //unable to remove the current entry
			}
		}
		return counter;
	}
}

int CZipArchive::deleteEntry(const string &e) const
{
	CZipEntry entry = getEntry(e);
	if (entry.isNull())
		return -4;

	return deleteEntry(entry);
}

int CZipArchive::renameEntry(const CZipEntry &entry, const string &newName) const
{
	if (!isOpen())
		return -3;

	if (entry.zipFile != this)
		return -3;

	if (mode == READ_ONLY)
		return -1;

	if (newName.length() == 0)
		return 0;

	if (newName == entry.getName())
		return 0;

	if (entry.isFile())
	{
		if (IS_DIRECTORY(newName))
			return 0;    //invalid new name

		int lastSlash = newName.rfind(DIRECTORY_SEPARATOR);
		if (lastSlash != 1)
		{
			bool dadded = addEntry(newName.substr(0, lastSlash + 1));
			if (!dadded)
				return 0;    //the hierarchy hasn't been created
		}
		
		int result = zip_file_rename(zipHandle, entry.getIndex(), AsciiToUtf8(newName).c_str(), DEFAULLT_ENC_FLAG);
		if (result == 0)
			return 1;

		return 0; //renaming was not possible (entry already exists ?)
	}
	else
	{
		if (!IS_DIRECTORY(newName))
			return 0;    //invalid new name

		int parentSlash = newName.rfind(DIRECTORY_SEPARATOR, newName.length() - 2);
		if (parentSlash != -1) //updates the dir hierarchy
		{
			string parent = newName.substr(0, parentSlash + 1);
			bool dadded = addEntry(parent);
			if (!dadded)
				return 0;
		}

		int counter = 0;
		string originalName = entry.getName();
		vector<CZipEntry> allEntries = getEntries();
		vector<CZipEntry>::const_iterator eit;
		for (eit = allEntries.begin() ; eit != allEntries.end() ; ++eit)
		{
			CZipEntry ze = *eit;
			string currentName = ze.getName();

			int startPosition = currentName.find(originalName);
			if (startPosition == 0)
			{
				if (currentName == originalName)
				{
					int result = zip_file_rename(zipHandle, entry.getIndex(), AsciiToUtf8(newName).c_str(), DEFAULLT_ENC_FLAG);
					if (result == 0)
						++counter;
					else
						return -2;     //unable to rename the folder
				}
				else
				{
					string targetName = currentName.replace(0, originalName.length(), newName);
					int result = zip_file_rename(zipHandle, ze.getIndex(), AsciiToUtf8(targetName).c_str(), DEFAULLT_ENC_FLAG);
					if (result == 0)
						++counter;
					else
						return -2;    //unable to rename a sub-entry
				}
			}
		}

		/*
		 * Special case for moving a directory a/x to a/x/y to avoid to lose
		 * the a/x path in the archive.
		 */
		bool newNameIsInsideCurrent = (newName.find(entry.getName()) == 0);
		if (newNameIsInsideCurrent)
		{
			bool dadded = addEntry(newName);
			if (!dadded)
				return 0;
		}

		return counter;
	}
}

int CZipArchive::renameEntry(const string &e, const string &newName) const
{
	CZipEntry entry = getEntry(e);
	if (entry.isNull())
		return -4;

	return renameEntry(entry, newName);
}

bool CZipArchive::addFile(const string &entryName, const string &file) const
{
	if (!isOpen())
		return false;

	if (mode == READ_ONLY)
		return false;    //adding not allowed

	if (IS_DIRECTORY(entryName))
		return false;

	int lastSlash = entryName.rfind(DIRECTORY_SEPARATOR);
	if (lastSlash != -1) //creates the needed parent directories
	{
		string dirEntry = entryName.substr(0, lastSlash + 1);
		bool dadded = addEntry(dirEntry);
		if (!dadded)
			return false;
	}

	FILE *fileSteam;
	zip_int64_t fileSize = -1;
	if (fopen_s(&fileSteam, file.c_str(), "rb+") == 0)
	{
		_fseeki64(fileSteam, 0, SEEK_END);
		fileSize =  _ftelli64(fileSteam);
		fclose(fileSteam);
	}

	zip_source *source = zip_source_file(zipHandle, file.c_str(), 0, fileSize);
	if (source != NULL)
	{
		zip_int64_t result = zip_file_add(zipHandle, AsciiToUtf8(entryName).c_str(), source, ZIP_FL_OVERWRITE);
		if (result >= 0)
			return true;
		else
			zip_source_free(source);    //unable to add the file
	}

	return false;
}

bool CZipArchive::addData(const string &entryName, const void *data, unsigned int length, bool freeData) const
{
	if (!isOpen())
		return false;

	if (mode == READ_ONLY)
		return false;    //adding not allowed

	if (IS_DIRECTORY(entryName))
		return false;

	int lastSlash = entryName.rfind(DIRECTORY_SEPARATOR);
	if (lastSlash != -1) //creates the needed parent directories
	{
		string dirEntry = entryName.substr(0, lastSlash + 1);
		bool dadded = addEntry(dirEntry);
		if (!dadded)
			return false;
	}

	zip_source *source = zip_source_buffer(zipHandle, data, length, freeData);
	if (source != NULL)
	{
		zip_int64_t result = zip_file_add(zipHandle, AsciiToUtf8(entryName).c_str(), source, ZIP_FL_OVERWRITE);
		if (result >= 0)
			return true;
		else
			zip_source_free(source);    //unable to add the file
	}

	return false;
}

bool CZipArchive::addEntry(const string &entryName) const
{
	if (!isOpen())
		return false;

	if (mode == READ_ONLY)
		return false;    //adding not allowed

	if (!IS_DIRECTORY(entryName))
		return false;

	int nextSlash = entryName.find(DIRECTORY_SEPARATOR);
	while (nextSlash != -1)
	{
		string pathToCreate = entryName.substr(0, nextSlash + 1);
		if (!hasEntry(pathToCreate))
		{
			zip_int64_t result = zip_dir_add(zipHandle, AsciiToUtf8(pathToCreate).c_str(), DEFAULLT_ENC_FLAG);
			if (result == -1)
				return false;
		}
		nextSlash = entryName.find(DIRECTORY_SEPARATOR, nextSlash + 1);
	}

	return true;
}

void CZipArchive::extract(const std::string &folderName)
{
	vector<CZipEntry> entries = getEntries(CURRENT);
	vector<CZipEntry>::iterator iterEntry = entries.begin(), iterEnd = entries.end();
	string extractPath;
	for (; iterEntry != iterEnd; iterEntry++)
	{
		if (iterEntry->isFile())
		{
			extractPath = concatPath(folderName, iterEntry->getName());
			createFolder(getFolderPath(extractPath));
			writeEntry(*iterEntry, extractPath);
		}
	}
}

bool CZipArchive::addFolder(const string &entryName, const string &folderName)
{
	WIN32_FIND_DATAA fd = {0};
	string strFind = concatPath(folderName, "*.*");
	HANDLE hFind = FindFirstFileA(strFind.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	string fileName;
	string fileEntryName;
	do
	{
		if(strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
			continue;

		fileName = concatPath(folderName, fd.cFileName);
		fileEntryName = concatPath(entryName, fd.cFileName, '/');
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			fileEntryName.push_back('/');
			if (!addFolder(fileEntryName, fileName))
			{
				FindClose(hFind);
				return FALSE;
			}
		}
		else
		{
			if (!addFile(fileEntryName, fileName))
			{
				FindClose(hFind);
				return false;
			}
		}

	}
	while (FindNextFileA(hFind, &fd));

	FindClose(hFind);
	return true;
}

std::string CZipArchive::concatPath(const std::string &strDir, const std::string &strFile, char slash /*= '\\'*/)
{
	if (strFile.empty())
		return strDir;

	string strDest = strDir;
	if (!strDest.empty())
	{
		char lastChar = strDest[strDest.size() - 1];
		if (lastChar != '\\' && lastChar != '/')
			strDest.push_back('\\');
	}

	if (strFile[0] == '\\' || strFile[0] == '/')
		strDest.append(strFile.substr(1));
	else
		strDest.append(strFile);

	for (string::size_type i = 0; i < strDest.size(); i++)
	{
		if (strDest[i] == '/' || strDest[i] == '\\')
			strDest[i] = slash;
	}

	return strDest;
}

std::string CZipArchive::getFolderPath(const std::string &filePath)
{
	for (string::size_type i = filePath.size() - 1; i > 0; i--)
	{
		if (filePath[i] == '\\' || filePath[i] == '/')
			return filePath.substr(0, i);
	}
}

void CZipArchive::createFolder(const std::string &folderName)
{
	char destPath[MAX_PATH] = { 0 };
	strcpy_s(destPath, MAX_PATH, folderName.c_str());

	int len = strlen(destPath);
	if (destPath[len - 1] != '\\' && destPath[len - 1] != '/')
		strcat_s(destPath, MAX_PATH, "\\");

	char createPath[MAX_PATH] = { 0 };
	for (int i = 0; i < strlen(destPath); ++i)
	{
		if (destPath[i] == '\\' || destPath[i] == '/')
		{
			memset(createPath, 0, MAX_PATH);
			strncpy_s(createPath, MAX_PATH, destPath, i);
			CreateDirectoryA(createPath, NULL);
		}
	}
}

void CZipArchive::TimetToFileTime(time_t t, LPFILETIME pft) const
{
	LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD) ll;
	pft->dwHighDateTime = ll >>32;
}