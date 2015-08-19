#include "Renderer.hpp"

Renderer::Renderer(CubemapSource* cubemapSource)
    :
	cubemapSource(cubemapSource), renderer(nullptr)
{
	std::function<StereoCubemap* (CubemapSource*, StereoCubemap*)> callback = boost::bind(&Renderer::onNextCubemap,
                                                                                          this,
                                                                                          _1,
                                                                                          _2);

	for (int i = 0; i < 1; i++)
	{
		cubemapPool.push(nullptr);
	}

	if (SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		abort();
	}

	// Now create a window and show it
	window = SDL_CreateWindow("AlloUnity - WindowedPlayer", 100, 100, 1000, 1000, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	// Make sure creating our window went ok
	if (window == nullptr)
	{
		std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		abort();
	}

	cubemapSource->setOnNextCubemap(callback);
}

Renderer::~Renderer()
{
	cubemapBuffer.close();
	cubemapPool.close();
	renderThread.join();

	//Clean up our objects and quit
	for (SDL_Texture* texture : textures)
	{
		SDL_DestroyTexture(texture);
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

StereoCubemap* Renderer::onNextCubemap(CubemapSource* source, StereoCubemap* cubemap)
{
	StereoCubemap* oldCubemap;
	//StereoCubemap::destroy(cubemap);
	if (!cubemapPool.try_pop(oldCubemap))
	{
		if (cubemapPool.closed())
		{
			return nullptr;
		}
		else
		{
			return cubemap;
		}
	}

	cubemapBuffer.push(cubemap);
	return oldCubemap;
	//if (onDisplayedFrame) onDisplayedFrame(this);
}

void Renderer::setOnDisplayedFrame(std::function<void (Renderer*)>& callback)
{
    onDisplayedFrame = callback;
}

void Renderer::setOnDisplayedCubemapFace(std::function<void (Renderer*, int)>& callback)
{
    onDisplayedCubemapFace = callback;
}

void Renderer::createTextures(StereoCubemap* cubemap)
{
	//Create a renderer that will draw to the window, -1 specifies that we want to load whichever
	//video driver supports the flags we're passing
	//Flags: SDL_RENDERER_ACCELERATED: We want to use hardware accelerated rendering
	//SDL_RENDERER_PRESENTVSYNC: We want the renderer's present function (update screen) to be
	//synchornized with the monitor's refresh rate
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		SDL_DestroyWindow(window);
		std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		abort();
	}

	for (int j = 0; j < cubemap->getEyesCount(); j++)
	{
		Cubemap* eye = cubemap->getEye(j);
		for (int i = 0; i < eye->getFacesCount(); i++)
		{
			CubemapFace* face = eye->getFace(i);
			SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, face->getContent()->getWidth(), face->getContent()->getHeight());
			if (texture == nullptr)
			{
				SDL_DestroyRenderer(renderer);
				SDL_DestroyWindow(window);
				std::cerr << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
				SDL_Quit();
				abort();
			}

			textures.push_back(texture);
		}
	}
}

void Renderer::start()
{
	renderThread = boost::thread(boost::bind(&Renderer::renderLoop, this));

	SDL_Event evt;
	while (true)
	{
		SDL_WaitEvent(&evt);
		if (evt.type == SDL_QUIT)
		{
			return;
		}
	}
}

void Renderer::renderLoop()
{
	static int counter = 0;

	while (true)
	{
		StereoCubemap* cubemap;

		if (!cubemapBuffer.wait_and_pop(cubemap))
		{
			return;
		}

		if (!renderer)
		{
			// Renderer and textures must be created on the thread where they are used
			createTextures(cubemap);
		}

		if (counter % 1 == 0)
		{

			//First clear the renderer
			SDL_RenderClear(renderer);

			for (int j = 0, textureIndex = 0; j < cubemap->getEyesCount(); j++)
			{
				Cubemap* eye = cubemap->getEye(j);
				for (int i = 0; i < eye->getFacesCount(); i++, textureIndex++)
				{

					Frame* content = eye->getFace(i)->getContent();
					SDL_Texture* texture = textures[textureIndex];

					// Show cubemap

				
					void* pixels;
					int   pitch;

					int width;
					int height;
					SDL_GetRendererOutputSize(renderer, &width, &height);

					SDL_Rect dstrect;

					dstrect.w = width / 4;
					dstrect.h = height / 6;

					switch (i)
					{
					case 1: // negative X
						dstrect.x = width * 0 / 4;
						dstrect.y = height / 3;
						break;
					case 4: // negative Z
						dstrect.x = width * 1 / 4;
						dstrect.y = height / 3;
						break;
					case 2: // postive Y
						dstrect.x = width * 1 / 4;
						dstrect.y = 0;
						break;
					case 5: // negative Y
						dstrect.x = width * 3 / 4;
						dstrect.y = height / 3;
						break;
					case 3: // positive Z
						dstrect.x = width * 1 / 4;
						dstrect.y = height * 2 / 3;
						break;
					case 0: // positive X
						dstrect.x = width * 2 / 4;
						dstrect.y = height / 3;
						break;
					default:
						dstrect.x = 0;
						dstrect.y = 0;
						dstrect.w = 0;
						dstrect.h = 0;
						break;
					}

					switch (j)
					{
					case 0:
						dstrect.y /= 2;
						break;
					case 1:
						dstrect.y /= 2;
						dstrect.y += height / 2;
					}

					if (SDL_LockTexture(texture, NULL, &pixels, &pitch) < 0)
					{
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
						SDL_Quit();
						abort();
					}
					memcpy(pixels, content->getPixels(), content->getHeight() * content->getWidth() * 4);
					SDL_UnlockTexture(texture);

					//Draw the texture
					if (SDL_RenderCopy(renderer, texture, NULL, &dstrect) < 0)
					{
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
						abort();
					}

					if (onDisplayedCubemapFace) onDisplayedCubemapFace(this, i + j * Cubemap::MAX_FACES_COUNT);

				}
			}

			SDL_RenderPresent(renderer);

			if (onDisplayedFrame) onDisplayedFrame(this);
		}

		cubemapPool.push(cubemap);
		counter++;
	}
}