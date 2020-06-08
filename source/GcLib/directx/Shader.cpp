#include "source/GcLib/pch.h"

#include "Shader.hpp"
#include "HLSL.hpp"

using namespace gstd;
using namespace directx;

/**********************************************************
//ShaderData
**********************************************************/
ShaderData::ShaderData() {
	bLoad_ = false;
	effect_ = nullptr;
	bText_ = false;
}
ShaderData::~ShaderData() {
	ptr_release(effect_);
	bLoad_ = false;
}

/**********************************************************
//ShaderManager
**********************************************************/
ShaderManager* ShaderManager::thisBase_ = nullptr;
ShaderManager::ShaderManager() {
	renderManager_ = nullptr;
}
ShaderManager::~ShaderManager() {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	graphics->RemoveDirectGraphicsListener(this);

	if (this == thisBase_) ptr_delete(renderManager_);

	Clear();
}
bool ShaderManager::Initialize() {
	if (thisBase_) return false;

	bool res = true;
	thisBase_ = this;
	DirectGraphics* graphics = DirectGraphics::GetBase();
	graphics->AddDirectGraphicsListener(this);

	renderManager_ = new RenderShaderManager();

	return res;
}
void ShaderManager::Clear() {
	{
		Lock lock(lock_);
		mapShader_.clear();
		mapShaderData_.clear();
	}
}
void ShaderManager::_ReleaseShaderData(std::wstring name) {
	{
		Lock lock(lock_);
		auto itr = mapShaderData_.find(name);
		if (itr != mapShaderData_.end()) {
			itr->second->bLoad_ = true;		//�ǂݍ��݊�������
			mapShaderData_.erase(itr);
			Logger::WriteTop(StringUtility::Format(L"ShaderManager�FShader released. [%s]", name.c_str()));
		}
	}
}
void ShaderManager::_ReleaseShaderData(std::map<std::wstring, shared_ptr<ShaderData>>::iterator itr) {
	{
		Lock lock(lock_);
		if (itr != mapShaderData_.end()) {
			std::wstring& name = itr->second->name_;
			itr->second->bLoad_ = true;		//�ǂݍ��݊�������
			mapShaderData_.erase(itr);
			Logger::WriteTop(StringUtility::Format(L"ShaderManager�FShader released. [%s]", name.c_str()));
		}
	}
}
bool ShaderManager::_CreateFromFile(std::wstring path, shared_ptr<ShaderData>& dest) {
	lastError_ = L"";
	auto itr = mapShaderData_.find(path);
	if (itr != mapShaderData_.end()) {
		dest = itr->second;
		return true;
	}

	//path = PathProperty::GetUnique(path);
	ref_count_ptr<FileReader> reader = FileManager::GetBase()->GetFileReader(path);
	if (reader == nullptr || !reader->Open()) {
		std::wstring log = StringUtility::Format(L"ShaderManager: Shader load failed. [%s]", path.c_str());
		Logger::WriteTop(log);
		lastError_ = log;
		return false;
	}

	size_t size = reader->GetFileSize();
	ByteBuffer buf;
	buf.SetSize(size);
	reader->Read(buf.GetPointer(), size);

	std::string source;
	source.resize(size);
	memcpy(&source[0], buf.GetPointer(), size);

	shared_ptr<ShaderData> data(new ShaderData());

	DirectGraphics* graphics = DirectGraphics::GetBase();
	ID3DXBuffer* pErr = nullptr;
	HRESULT hr = D3DXCreateEffect(
		graphics->GetDevice(),
		source.c_str(),
		source.size(),
		nullptr, nullptr,
		0,
		nullptr,
		&data->effect_,
		&pErr
		);

	bool res = true;
	if (FAILED(hr)) {
		res = false;
		std::wstring err = L"";
		if (pErr) {
			char* cText = (char*)pErr->GetBufferPointer();
			err = StringUtility::ConvertMultiToWide(cText);
		}
		std::wstring log = StringUtility::Format(L"ShaderManager: Shader load failed. [%s]\r\n%s", path.c_str(), err.c_str());
		Logger::WriteTop(log);
		lastError_ = log;
	}
	else {
		std::wstring log = StringUtility::Format(L"ShaderManager: Shader loaded. [%s]", path.c_str());
		Logger::WriteTop(log);

		mapShaderData_[path] = data;
		data->manager_ = this;
		data->name_ = path;

		dest = data;
	}

	return res;
}
bool ShaderManager::_CreateFromText(std::string& source, shared_ptr<ShaderData>& dest) {
	lastError_ = L"";
	std::wstring id = _GetTextSourceID(source);
	auto itr = mapShaderData_.find(id);
	if (itr != mapShaderData_.end()) {
		dest = itr->second;
		return true;
	}

	bool res = true;
	DirectGraphics* graphics = DirectGraphics::GetBase();

	shared_ptr<ShaderData> data(new ShaderData());
	ID3DXBuffer* pErr = nullptr;
	HRESULT hr = D3DXCreateEffect(
		graphics->GetDevice(),
		source.c_str(),
		source.size(),
		nullptr, nullptr,
		0,
		nullptr,
		&data->effect_,
		&pErr
		);

	std::string tStr = StringUtility::Slice(source, 128);
	if (FAILED(hr)) {
		res = false;
		char* err = nullptr;
		if (pErr) err = (char*)pErr->GetBufferPointer();
		std::string log = StringUtility::Format("ShaderManager: Shader load failed. [%s]\r\n%s", tStr.c_str(), err);
		Logger::WriteTop(log);
		lastError_ = StringUtility::ConvertMultiToWide(log);
	}
	else {
		std::wstring log = StringUtility::Format(L"ShaderManager: Shader loaded. [%s]", tStr.c_str());
		Logger::WriteTop(log);

		mapShaderData_[id] = data;
		data->manager_ = this;
		data->name_ = id;
		data->bText_ = true;

		dest = data;
	}

	return res;
}
std::wstring ShaderManager::_GetTextSourceID(std::string& source) {
	std::wstring res = StringUtility::ConvertMultiToWide(source);
	res = StringUtility::Slice(res, 64);
	return res;
}

void ShaderManager::ReleaseDxResource() {
	std::map<std::wstring, shared_ptr<ShaderData>>::iterator itrMap;
	{
		Lock lock(lock_);

		for (itrMap = mapShaderData_.begin(); itrMap != mapShaderData_.end(); ++itrMap) {
			shared_ptr<ShaderData> data = itrMap->second;
			data->effect_->OnLostDevice();
		}
		renderManager_->OnLostDevice();
	}
}
void ShaderManager::RestoreDxResource() {
	std::map<std::wstring, shared_ptr<ShaderData>>::iterator itrMap;
	{
		Lock lock(lock_);

		for (itrMap = mapShaderData_.begin(); itrMap != mapShaderData_.end(); ++itrMap) {
			shared_ptr<ShaderData> data = itrMap->second;
			data->effect_->OnResetDevice();
		}
		renderManager_->OnResetDevice();
	}
}

bool ShaderManager::IsDataExists(std::wstring name) {
	bool res = false;
	{
		Lock lock(lock_);
		res = mapShaderData_.find(name) != mapShaderData_.end();
	}
	return res;
}
std::map<std::wstring, shared_ptr<ShaderData>>::iterator ShaderManager::IsDataExistsItr(std::wstring& name) {
	auto res = mapShaderData_.end();
	{
		Lock lock(lock_);
		res = mapShaderData_.find(name);
	}
	return res;
}
shared_ptr<ShaderData> ShaderManager::GetShaderData(std::wstring name) {
	shared_ptr<ShaderData> res;
	{
		Lock lock(lock_);
		auto itr = mapShaderData_.find(name);
		if (itr != mapShaderData_.end()) {
			res = itr->second;
		}
	}
	return res;
}
shared_ptr<Shader> ShaderManager::CreateFromFile(std::wstring path) {
	//path = PathProperty::GetUnique(path);
	shared_ptr<Shader> res = nullptr;
	{
		Lock lock(lock_);
		auto itr = mapShader_.find(path);
		if (itr != mapShader_.end()) {
			res = itr->second;
		}
		else {
			shared_ptr<ShaderData> data = nullptr;
			if (_CreateFromFile(path, data)) {
				res = std::make_shared<Shader>();
				res->data_ = data;

				//mapShader_[path] = res;
			}
		}
	}
	return res;
}
shared_ptr<Shader> ShaderManager::CreateFromText(std::string source) {
	shared_ptr<Shader> res = nullptr;
	{
		Lock lock(lock_);
		std::wstring id = _GetTextSourceID(source);

		auto itr = mapShader_.find(id);
		if (itr != mapShader_.end()) {
			res = itr->second;
		}
		else {
			shared_ptr<ShaderData> data = nullptr;
			if (_CreateFromText(source, data)) {
				res = std::make_shared<Shader>();
				res->data_ = data;

				//mapShader_[id] = res;
			}
		}
	}
	return res;
}
shared_ptr<Shader> ShaderManager::CreateFromFileInLoadThread(std::wstring path) {
	return false;
}
void ShaderManager::CallFromLoadThread(shared_ptr<gstd::FileManager::LoadThreadEvent> event) {
}

void ShaderManager::AddShader(std::wstring name, shared_ptr<Shader> shader) {
	{
		Lock lock(lock_);
		mapShader_[name] = shader;
	}
}
void ShaderManager::DeleteShader(std::wstring name) {
	{
		Lock lock(lock_);
		mapShader_.erase(name);
	}
}
shared_ptr<Shader> ShaderManager::GetShader(std::wstring name) {
	shared_ptr<Shader> res = nullptr;
	{
		Lock lock(lock_);
		auto itr = mapShader_.find(name);
		if (itr != mapShader_.end()) {
			res = itr->second;
		}
	}
	return res;
}
std::wstring ShaderManager::GetLastError() {
	std::wstring res;
	{
		Lock lock(lock_);
		res = lastError_;
	}
	return res;
}

/**********************************************************
//ShaderParameter
**********************************************************/
ShaderParameter::ShaderParameter() {
	type_ = TYPE_UNKNOWN;
	texture_ = nullptr;
}
ShaderParameter::~ShaderParameter() {
}
void ShaderParameter::SetMatrix(D3DXMATRIX& matrix) {
	type_ = TYPE_MATRIX;

	value_.resize(sizeof(D3DXMATRIX));
	memcpy(value_.data(), &matrix, sizeof(D3DXMATRIX));
}
D3DXMATRIX* ShaderParameter::GetMatrix() {
	return (D3DXMATRIX*)(value_.data());
}
void ShaderParameter::SetMatrixArray(std::vector<D3DXMATRIX>& listMatrix) {
	type_ = TYPE_MATRIX_ARRAY;

	value_.resize(listMatrix.size() * sizeof(D3DXMATRIX));
	memcpy(value_.data(), listMatrix.data(), listMatrix.size() * sizeof(D3DXMATRIX));
}
std::vector<D3DXMATRIX> ShaderParameter::GetMatrixArray() {
	std::vector<D3DXMATRIX> res;
	res.resize(value_.size() / sizeof(D3DXMATRIX));
	memcpy(res.data(), value_.data(), res.size() * sizeof(D3DXMATRIX));
	return res;
}
void ShaderParameter::SetVector(D3DXVECTOR4& vector) {
	type_ = TYPE_VECTOR;

	value_.resize(sizeof(D3DXVECTOR4));
	memcpy(value_.data(), &vector, sizeof(D3DXVECTOR4));
}
D3DXVECTOR4* ShaderParameter::GetVector() {
	return (D3DXVECTOR4*)(value_.data());
}
void ShaderParameter::SetFloat(FLOAT value) {
	type_ = TYPE_FLOAT;

	value_.resize(sizeof(FLOAT));
	memcpy(value_.data(), &value, sizeof(FLOAT));
}
FLOAT* ShaderParameter::GetFloat() {
	return (FLOAT*)(value_.data());
}
void ShaderParameter::SetFloatArray(std::vector<FLOAT>& values) {
	type_ = TYPE_FLOAT_ARRAY;

	value_.resize(values.size() * sizeof(FLOAT));
	memcpy(value_.data(), values.data(), values.size() * sizeof(FLOAT));
}
std::vector<FLOAT> ShaderParameter::GetFloatArray() {
	std::vector<FLOAT> res;
	res.resize(value_.size() / sizeof(FLOAT));
	memcpy(res.data(), value_.data(), res.size() * sizeof(FLOAT));
	return res;
}
void ShaderParameter::SetTexture(shared_ptr<Texture> texture) {
	type_ = TYPE_TEXTURE;
	texture_ = texture;
}
shared_ptr<Texture> ShaderParameter::GetTexture() {
	return texture_;
}

/**********************************************************
//Shader
**********************************************************/
Shader::Shader() {
	data_ = nullptr;
}
Shader::Shader(Shader* shader) {
	{
		Lock lock(ShaderManager::GetBase()->GetLock());
		data_ = shader->data_;
	}
}
Shader::~Shader() {
	Release();
}
void Shader::Release() {
	{
		Lock lock(ShaderManager::GetBase()->GetLock());
		if (data_) {
			ShaderManager* manager = data_->manager_;
			if (manager) {
				auto itrData = manager->IsDataExistsItr(data_->name_);

				//No other references other than the one here and the one in ShaderManager
				if (data_.use_count() == 2) {
					manager->_ReleaseShaderData(itrData);
				}
			}
			data_ = nullptr;
		}
	}
}

ID3DXEffect* Shader::GetEffect() {
	ID3DXEffect* res = nullptr;
	if (data_) res = data_->effect_;
	return res;
}
bool Shader::CreateFromFile(std::wstring path) {
	//path = PathProperty::GetUnique(path);

	bool res = false;
	{
		Lock lock(ShaderManager::GetBase()->GetLock());
		if (data_ != nullptr)Release();
		ShaderManager* manager = ShaderManager::GetBase();
		shared_ptr<Shader> shader = manager->CreateFromFile(path);
		if (shader) {
			data_ = shader->data_;
		}
		res = data_ != nullptr;
	}

	return res;
}
bool Shader::CreateFromText(std::string& source) {
	bool res = false;
	{
		Lock lock(ShaderManager::GetBase()->GetLock());
		if (data_) Release();
		ShaderManager* manager = ShaderManager::GetBase();
		shared_ptr<Shader> shader = manager->CreateFromText(source);
		if (shader) {
			data_ = shader->data_;
		}
		res = data_ != nullptr;
	}

	return res;
}

bool Shader::LoadParameter() {
	ID3DXEffect* effect = GetEffect();
	if (effect == nullptr) return false;

	HRESULT hr = effect->SetTechnique(technique_.c_str());
	if (FAILED(hr))return false;

	for (auto itrParam = mapParam_.begin(); itrParam != mapParam_.end(); ++itrParam) {
		std::string name = itrParam->first;
		shared_ptr<ShaderParameter> param = itrParam->second;

		switch (param->GetType()) {
		case ShaderParameter::TYPE_MATRIX:
		{
			D3DXMATRIX* matrix = param->GetMatrix();
			hr = effect->SetMatrix(name.c_str(), matrix);
			break;
		}
		case ShaderParameter::TYPE_MATRIX_ARRAY:
		{
			//std::vector<D3DXMATRIX> matrixArray = param->GetMatrixArray();
			//hr = effect->SetMatrixArray(name.c_str(), &matrixArray[0], matrixArray.size());
			std::vector<byte>* raw = param->GetRaw();
			hr = effect->SetMatrixArray(name.c_str(), 
				(D3DXMATRIX*)raw->data(), raw->size()/ sizeof(D3DXMATRIX));
			break;
		}
		case ShaderParameter::TYPE_VECTOR:
		{
			D3DXVECTOR4* vect = param->GetVector();
			hr = effect->SetVector(name.c_str(), vect);
			break;
		}
		case ShaderParameter::TYPE_FLOAT:
		{
			hr = effect->SetFloat(name.c_str(), *param->GetFloat());
			break;
		}
		case ShaderParameter::TYPE_FLOAT_ARRAY:
		{
			//std::vector<float> value = param->GetFloatArray();
			//hr = effect->SetFloatArray(name.c_str(), &value[0], value.size());
			std::vector<byte>* raw = param->GetRaw();
			hr = effect->SetFloatArray(name.c_str(),
				(FLOAT*)raw->data(), raw->size() / sizeof(FLOAT));
			break;
		}
		case ShaderParameter::TYPE_TEXTURE:
		{
			IDirect3DTexture9* pTex = param->GetTexture()->GetD3DTexture();
			hr = effect->SetTexture(name.c_str(), pTex);
			break;
		}
		}
		//if(FAILED(hr))return false;
	}
	return true;
}
shared_ptr<ShaderParameter> Shader::_GetParameter(std::string name, bool bCreate) {
	auto itr = mapParam_.find(name);
	bool bFind = itr != mapParam_.end();
	if (!bFind && !bCreate) return nullptr;

	if (!bFind) {
		shared_ptr<ShaderParameter> res(new ShaderParameter());
		mapParam_[name] = res;
		return res;
	}
	else {
		return itr->second;
	}
}

bool Shader::SetTechnique(std::string name) {
	technique_ = name;
	return true;
}
bool Shader::SetMatrix(std::string name, D3DXMATRIX& matrix) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetMatrix(matrix);
	return true;
}
bool Shader::SetMatrixArray(std::string name, std::vector<D3DXMATRIX>& matrix) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetMatrixArray(matrix);
	return true;
}
bool Shader::SetVector(std::string name, D3DXVECTOR4& vector) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetVector(vector);
	return true;
}
bool Shader::SetFloat(std::string name, FLOAT value) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetFloat(value);
	return true;
}
bool Shader::SetFloatArray(std::string name, std::vector<FLOAT>& values) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetFloatArray(values);
	return true;
}
bool Shader::SetTexture(std::string name, shared_ptr<Texture> texture) {
	shared_ptr<ShaderParameter> param = _GetParameter(name, true);
	param->SetTexture(texture);
	return true;
}

/**********************************************************
//RenderShaderManager
**********************************************************/
RenderShaderManager::RenderShaderManager() {
	listEffect_.resize(4, nullptr);
	listDeclaration_.resize(6, nullptr);
	Initialize();
}
RenderShaderManager::~RenderShaderManager() {
	Release();
}

void RenderShaderManager::Initialize() {
	ID3DXBuffer* error = nullptr;

	DirectGraphics* graphics = DirectGraphics::GetBase();
	IDirect3DDevice9* device = graphics->GetDevice();

	HRESULT hr = S_OK;

	{
		std::pair<const std::string*, const std::string*> listCreate[6] = {
			std::make_pair(&HLSL_DEFAULT_SKINNED_MESH, &NAME_DEFAULT_SKINNED_MESH),
			std::make_pair(&HLSL_DEFAULT_RENDER2D, &NAME_DEFAULT_RENDER2D),
			std::make_pair(&HLSL_DEFAULT_HWINSTANCE2D, &NAME_DEFAULT_HWINSTANCE2D),
			std::make_pair(&HLSL_DEFAULT_HWINSTANCE3D, &NAME_DEFAULT_HWINSTANCE3D)
		};
		for (size_t iEff = 0U; iEff < 4U; ++iEff) {
			const std::string* ptrSrc = listCreate[iEff].first;

			hr = D3DXCreateEffect(device, ptrSrc->c_str(), ptrSrc->size(), nullptr, nullptr, 0, 
				nullptr, &listEffect_[iEff], &error);
			if (FAILED(hr)) {
				std::string err = StringUtility::Format("RenderShaderManager: Shader load failed. (%s)\n[%s]",
					listCreate[iEff].second->c_str(), reinterpret_cast<const char*>(error->GetBufferPointer()));
				throw gstd::wexception(err);
			}
		}
	}
	listEffect_[1]->SetTechnique("Render");

	{
		std::pair<const D3DVERTEXELEMENT9*, std::string> listCreate[6] = {
			std::make_pair(ELEMENTS_TLX, "ELEMENTS_TLX"),
			std::make_pair(ELEMENTS_LX, "ELEMENTS_LX"),
			std::make_pair(ELEMENTS_NX, "ELEMENTS_NX"),
			std::make_pair(ELEMENTS_BNX, "ELEMENTS_BNX"),
			std::make_pair(ELEMENTS_TLX_INSTANCED, "ELEMENTS_TLX_INSTANCED"),
			std::make_pair(ELEMENTS_LX_INSTANCED, "ELEMENTS_LX_INSTANCED")
		};
		for (size_t iDecl = 0U; iDecl < 6U; ++iDecl) {
			hr = device->CreateVertexDeclaration(listCreate[iDecl].first, &listDeclaration_[iDecl]);
			if (FAILED(hr)) {
				std::string err = StringUtility::Format("RenderShaderManager: CreateVertexDeclaration failed. (%s)",
					listCreate[iDecl].second.c_str());
				throw gstd::wexception(err);
			}
		}
	}
}
void RenderShaderManager::Release() {
	for (auto& iEffect : listEffect_) ptr_release(iEffect);
	for (auto& iDecl : listDeclaration_) ptr_release(iDecl);
}

void RenderShaderManager::OnLostDevice() {
	for (auto& iEffect : listEffect_) 
		iEffect->OnLostDevice();
}
void RenderShaderManager::OnResetDevice() {
	for (auto& iEffect : listEffect_) 
		iEffect->OnResetDevice();
}