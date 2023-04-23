# Milwaukee

A simple software renderer for experimentation and education, Milwuakee is the codename for my the shared codebase for rendering projects that do not use the GPU for any rendering logic beyond the final swapchain drawing (although whether by a Pixel Buffer Object, through Compute Shader, Bit blit to a texture or framebuffer depends on the project). Consider it a potential 'CPU Game Engine' if you need to. It uses the OpenGL API and GLFW in order to interact with a framebuffer but no rasterization or raytracing will be done on the GPU.
