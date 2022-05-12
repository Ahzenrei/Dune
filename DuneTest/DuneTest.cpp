#include <Dune.h>

class DuneLauncher : public Dune::Application
{
public:
	DuneLauncher()
	{

	}

	~DuneLauncher()
	{

	}

	void Test1()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			Dune::EntityManager* pEntityManager = Dune::EngineCore::GetEntityManager();
			srand((unsigned int)time(NULL));

			for (int i = 0; i < 1000; i++)
			{
				Dune::EntityID entity = Dune::EngineCore::CreateEntity("Node");
				if ((rand() % 11) < 3)
				{
					pEntityManager->RemoveEntity(entity);
				}
			}
			didTestOnce = true;
		}
	}

	void Test2()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			Dune::GraphicsRenderer& graphicsRenderer = Dune::GraphicsCore::GetGraphicsRenderer();

			// Define the geometry for a triangle.
			Dune::dVector<Dune::Vertex> triangleVertices =
			{
				{ { 0.25f, -0.25f * 1.77f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
				{ { 0.0f, 0.25f * 1.77f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
				{ { -0.25f, -0.25f * 1.77f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
			};
			Dune::dVector<uint32_t> indices = {};
			Dune::Mesh* mesh = new Dune::Mesh(indices, triangleVertices);
			mesh->UploadBuffers();
			Dune::dString path = "bla";
			Dune::Shader* shader = new Dune::Shader(path);
			Dune::GraphicsElement* elem = new Dune::GraphicsElement(*mesh, *shader);

			graphicsRenderer.AddGraphicsElement(*elem);

			didTestOnce = true;
		}
	}

	void OnUpdate() override
	{
		Test1();
		Test2();
	}

};

int main(int argc, char** argv)
{
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}