#pragma once

namespace Dune
{
	class Shader
	{
	public:
		Shader(const std::string& path);
	private:
		std::string m_path;
	};
}