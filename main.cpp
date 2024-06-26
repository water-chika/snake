#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#include <vulkan_helper.hpp>
#include <iostream>
#include <numeric>
#include <map>
#include <string>

template<class T>
class rename_images : public T {
public:
	using parent = T;
	auto get_intermediate_images() {
		return parent::get_images();
	}
};

template<class T>
class record_swapchain_command_buffers_cube : public T {
public:
	using parent = T;
	record_swapchain_command_buffers_cube() {
		create();
	}
	void create() {
		auto buffers = parent::get_swapchain_command_buffers();
		auto swapchain_images = parent::get_swapchain_images();
		auto queue_family_index = parent::get_queue_family_index();
		auto framebuffers = parent::get_framebuffers();
		std::vector<vk::Buffer> uniform_buffers = parent::get_uniform_buffer_vector();
		std::vector<vk::Buffer> uniform_upload_buffers = parent::get_uniform_upload_buffer_vector();
		std::vector<vk::DescriptorSet> descriptor_sets = parent::get_descriptor_set();

		auto clear_color_value_type = parent::get_format_clear_color_value_type(parent::get_swapchain_image_format());
		using value_type = decltype(clear_color_value_type);
		std::map<value_type, vk::ClearColorValue> clear_color_values{
			{value_type::eFloat32, vk::ClearColorValue{}.setFloat32({0.4f,0.4f,0.4f,0.0f})},
			{value_type::eUint32, vk::ClearColorValue{}.setUint32({50,50,50,0})},
		};
		if (!clear_color_values.contains(clear_color_value_type)) {
			throw std::runtime_error{ "unsupported clear color value type" };
		}
		vk::ClearColorValue clear_color_value{ clear_color_values[clear_color_value_type] };
		auto clear_depth_value = vk::ClearDepthStencilValue{}.setDepth(1.0f);
		auto clear_values = std::array{
			vk::ClearValue{}.setColor(clear_color_value),
			vk::ClearValue{}.setDepthStencil(clear_depth_value)
		};

		if (buffers.size() != swapchain_images.size()) {
			throw std::runtime_error{ "swapchain images count != command buffers count" };
		}
		uint32_t index = 0;
		for (uint32_t index = 0; index < buffers.size(); index++) {
			vk::Image swapchain_image = swapchain_images[index];
			vk::CommandBuffer cmd = buffers[index];

			cmd.begin(
				vk::CommandBufferBeginInfo{}
			);

			vk::Buffer uniform_buffer = uniform_buffers[index];
			vk::Buffer upload_buffer = uniform_upload_buffers[index];
			cmd.copyBuffer(upload_buffer, uniform_buffer,
				vk::BufferCopy{}.setSize(sizeof(uint64_t)));

			vk::RenderPass render_pass = parent::get_render_pass();

			vk::Extent2D swapchain_image_extent = parent::get_swapchain_image_extent();
			auto render_area = vk::Rect2D{}.setOffset(vk::Offset2D{ 0,0 }).setExtent(swapchain_image_extent);
			vk::Framebuffer framebuffer = framebuffers[index];
			cmd.beginRenderPass(
				vk::RenderPassBeginInfo{}
				.setRenderPass(render_pass)
				.setRenderArea(render_area)
				.setFramebuffer(framebuffer)
				.setClearValues(
					clear_values
				),
				vk::SubpassContents::eInline
			);
			
			vk::Pipeline pipeline = parent::get_pipeline();
			cmd.bindPipeline(
				vk::PipelineBindPoint::eGraphics,
				pipeline
			);
			vk::Buffer vertex_buffer = parent::get_vertex_buffer_vector()[index];
			cmd.bindVertexBuffers(0,
				vertex_buffer, vk::DeviceSize{ 0 });
			vk::Buffer index_buffer = parent::get_index_buffer();
			cmd.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint16);

			vk::PipelineLayout pipeline_layout = parent::get_pipeline_layout();
			vk::DescriptorSet descriptor_set = descriptor_sets[index];
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_set, {});
			cmd.drawIndexed(3 * 2 * 3 * 2, 1, 0, 0, 0);
			cmd.endRenderPass();
			cmd.end();
		}
	}
	void destroy() {

	}
};

template<class T>
class add_compute_pipeline : public T {
public:
	using parent = T;
	add_compute_pipeline() {
		create();
	}
	~add_compute_pipeline() {
		destroy();
	}
	void create() {
		vk::Device device = parent::get_device();
		vk::PipelineShaderStageCreateInfo stage = parent::get_shader_stage();

		m_pipeline = device.createComputePipeline(
			nullptr,
			vk::ComputePipelineCreateInfo{}
			.setStage(stage)
		);
	}
	void destroy() {
		vk::Device device = parent::get_device();
		device.destroyPipeline(m_pipeline);
	}
private:
	vk::Pipeline m_pipeline;
};

namespace windows_helper {
	template<class T>
	class add_window_process : public T {
	public:
		static LRESULT CALLBACK window_process(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
			static std::map<HWND, add_window_process<T>*> hwnd_this{};
			switch (uMsg)
			{
			case WM_CREATE:
			{
				auto create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
				hwnd_this.emplace(hwnd, reinterpret_cast<add_window_process<T>*>(create_struct->lpCreateParams));
				break;
			}
			case WM_SIZE:
			{
				uint16_t width = lParam;
				uint16_t height = lParam >> 16;
				hwnd_this[hwnd]->set_size(width, height);
				break;
			}
			case WM_DESTROY:
			{
				PostQuitMessage(0);
				break;
			}
			}
			return DefWindowProcA(hwnd, uMsg, wParam, lParam);
		}
		void* get_lparam() {
			return this;
		}
		void set_size(uint16_t width, uint16_t height) {
			m_width = width;
			m_height = height;
		}
		bool is_window_minimized() {
			return m_width == 0 || m_height == 0;
		}
	private:
		static add_window_process<T>* m_this;
		uint16_t m_width;
		uint16_t m_height;
	};
	template<class T>
	class add_window_class : public T {
	public:
		using parent = T;
		add_window_class() {
			const char* window_class_name = "draw_pixels";
			WNDCLASS window_class{};
			window_class.hInstance = GetModuleHandle(NULL);
			window_class.lpszClassName = window_class_name;
			window_class.lpfnWndProc = parent::window_process;
			m_window_class = RegisterClass(&window_class);
		}
		~add_window_class() {
			UnregisterClass((LPCSTR)m_window_class, GetModuleHandle(NULL));
		}
		auto get_window_class() {
			return m_window_class;
		}
	private:
		ATOM m_window_class;

	};
	template<int Width, int Height, class T>
	class set_window_resolution : public T {
	public:
		auto get_window_width() {
			return Width;
		}
		auto get_window_height() {
			return Height;
		}
	};
	template<int WindowStyle, class T>
	class set_window_style : public T {
	public:
		auto get_window_style() {
			return WindowStyle;
		}
	};
	template<class T>
	class adjust_window_resolution : public T {
	public:
		using parent = T;
		adjust_window_resolution() {
			auto width = parent::get_window_width();
			auto height = parent::get_window_height();
			auto window_style = parent::get_window_style();
			RECT rect = { 0,0,width,height };
			AdjustWindowRect(&rect, window_style, false);
			m_width = rect.right - rect.left;
			m_height = rect.bottom - rect.top;
		}
		auto get_window_width() {
			return m_width;
		}
		auto get_window_height() {
			return m_height;
		}
	private:
		int m_width;
		int m_height;
	};
	template<class T>
	class add_window : public T {
	public:
		using parent = T;
		add_window() {
			int width = parent::get_window_width();
			int height = parent::get_window_height();
			int window_style = parent::get_window_style();
			m_window = CreateWindowA(
				(LPCSTR)parent::get_window_class(), "snake", window_style, CW_USEDEFAULT, CW_USEDEFAULT,
				width, height, NULL, NULL, GetModuleHandle(NULL), parent::get_lparam());
			if (m_window == NULL) {
				throw std::runtime_error("failed to create window");
			}
			ShowWindow(m_window, SW_SHOWNORMAL);
		}
		auto get_window() {
			return m_window;
		}
	private:
		HWND m_window;
	};
	template<class T>
	class add_window_loop : public T {
	public:
		using parent = T;
		add_window_loop() {
			MSG msg = { };
			while (msg.message != WM_QUIT)
			{
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				else {
					parent::draw();
				}
			}
		}
	};

}

namespace vulkan_windows_helper {

	template<class T>
	class add_windows_surface : public T {
	public:
		using parent = T;
		add_windows_surface() {
			create_surface();
		}
		~add_windows_surface() {
			destroy_surface();
		}
		void create_surface() {
			vk::Instance instance = parent::get_instance();
			m_surface = instance.createWin32SurfaceKHR(
				vk::Win32SurfaceCreateInfoKHR{}
				.setHinstance(GetModuleHandleA(NULL))
				.setHwnd(parent::get_window())
			);
		}
		void destroy_surface() {
			vk::Instance instance = parent::get_instance();
			instance.destroySurfaceKHR(m_surface);
		}
		auto get_surface() {
			return m_surface;
		}
	private:
		vk::SurfaceKHR m_surface;
	};
}

using namespace vulkan_hpp_helper;
using namespace vulkan_windows_helper;
using namespace windows_helper;

template<class T>
class add_triangle_vertex_buffer_data : public T {
public:
	auto get_buffer_size() {
		return m_data.size() * sizeof(m_data[0]);
	}
	auto get_buffer_data() {
		return m_data;
	}
private:
	static constexpr auto m_data = std::array{
		-0.5f, -0.5f,
					0.0f, 0.0f,
		0.5f, -0.5f,

	};
};

	template<class T>
	class add_cube_vertex_buffer_data : public T {
	public:
		auto get_buffer_size() {
			return m_data.size() * sizeof(m_data[0]);
		}
		auto get_buffer_data() {
			return m_data;
		}
	private:
		static constexpr auto m_data = std::array{
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
		};
	};
	template<class T>
	class add_cube_index_buffer_data : public T {
	public:
		using parent = T;
		auto get_buffer_size() {
			return m_data.size() * sizeof(m_data[0]);
		}
		auto get_buffer_data() {
			return m_data;
		}
	private:
		static constexpr auto m_data = std::array<uint16_t, 3*3*2*2>{
				0, 1, 3,
				0, 3, 2,
				4, 6, 7,
				4, 7, 5,
				1, 5, 7,
				1, 7, 3,
				3, 7, 6,
				3, 6, 2,
				2, 6, 4,
				2, 4, 0,
				0, 4, 5,
				0, 5, 1,
		};
	};
	template<class T>
	class add_cube_vertex_shader_path : public T {
	public:
		auto get_file_path() {
			return std::filesystem::path{ "cube_vert.spv" };
		}
	};
	template<class T>
	class add_cube_fragment_shader_path : public T {
	public:
		auto get_file_path() {
			return std::filesystem::path{ "cube_frag.spv" };
		}
	};
	template<class T>
	class add_cube_descriptor_set_layout_binding : public T {
	public:
		using parent = T;
		add_cube_descriptor_set_layout_binding() {
			vk::ShaderStageFlagBits stages = vk::ShaderStageFlagBits::eVertex;
			m_binding = vk::DescriptorSetLayoutBinding{}
				.setBinding(0)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setStageFlags(stages);
		}
		auto get_descriptor_set_layout_bindings() {
			return m_binding;
		}
	private:
		vk::DescriptorSetLayoutBinding m_binding;
	};
	template<class T>
	class add_descriptor_pool : public T {
	public:
		using parent = T;
		add_descriptor_pool() {
			create();
		}
		~add_descriptor_pool() {
			destroy();
		}
		void create() {
			vk::Device device = parent::get_device();
			uint32_t count = parent::get_swapchain_images().size();
			auto pool_sizes = vk::DescriptorPoolSize{}
				.setDescriptorCount(count)
				.setType(vk::DescriptorType::eUniformBuffer);
			m_pool = device.createDescriptorPool(
				vk::DescriptorPoolCreateInfo{}
				.setMaxSets(count)
				.setPoolSizes(pool_sizes)
			);
		}
		void destroy() {
			vk::Device device = parent::get_device();

			device.destroyDescriptorPool(m_pool);
		}
		auto get_descriptor_pool() {
			return m_pool;
		}
	private:
		vk::DescriptorPool m_pool;
	};
	template<class T>
	class add_nonfree_descriptor_set : public T {
	public:
		using parent = T;
		add_nonfree_descriptor_set() {
			create();
		}
		~add_nonfree_descriptor_set() {
			destroy();
		}
		void create() {
			vk::Device device = parent::get_device();
			vk::DescriptorPool pool = parent::get_descriptor_pool();
			vk::DescriptorSetLayout layout = parent::get_descriptor_set_layout();
			uint32_t count = parent::get_swapchain_images().size();
			std::vector<vk::DescriptorSetLayout> layouts(count);
			std::ranges::for_each(layouts, [layout](auto& l) {l = layout; });
			m_set = device.allocateDescriptorSets(
				vk::DescriptorSetAllocateInfo{}
				.setDescriptorPool(pool)
				.setDescriptorSetCount(count)
				.setSetLayouts(layouts)
			);
		}
		void destroy() {
		}
		auto get_descriptor_set() {
			return m_set;
		}
	private:
		std::vector<vk::DescriptorSet> m_set;
	};
	template<class T>
	class set_vector_size_to_swapchain_image_count : public T {
	public:
		using parent = T;
		auto get_vector_size() {
			return parent::get_swapchain_images().size();
		}
	};
	template<class T>
	class write_descriptor_set : public T {
	public:
		using parent = T;
		write_descriptor_set() {
			create();
		}
		auto create() {
			vk::Device device = parent::get_device();
			std::vector<vk::Buffer> buffers = parent::get_uniform_buffer_vector();
			std::vector<vk::DescriptorSet> sets = parent::get_descriptor_set();

			std::vector<vk::WriteDescriptorSet> writes(sets.size());
			for (uint32_t i = 0; i < sets.size(); i++) {
				auto buffer = buffers[i];
				auto set = sets[i];
				auto& write = writes[i];

				auto buffer_info = vk::DescriptorBufferInfo{}
					.setBuffer(buffer)
					.setRange(vk::WholeSize);
				write = 
					vk::WriteDescriptorSet{}
					.setDstSet(set)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDstBinding(0)
					.setBufferInfo(buffer_info);
			}
			device.updateDescriptorSets(writes, {});
		}
	};
	template<class T>
	class add_framebuffers_cube : public T {
	public:
		using parent = T;
		add_framebuffers_cube() {
			create_framebuffers();
		}
		~add_framebuffers_cube() {
			destroy_framebuffers();
		}
		void create_framebuffers() {
			vk::Device device = parent::get_device();
			vk::RenderPass render_pass = parent::get_render_pass();
			auto extent = parent::get_swapchain_image_extent();
			uint32_t width = extent.width;
			uint32_t height = extent.height;
			auto swapchain_image_views = parent::get_swapchain_image_views();
			auto depth_image_views = parent::get_depth_images_views();
			m_framebuffers.resize(swapchain_image_views.size());
			for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
				auto depth_image_view = depth_image_views[i];
				auto swapchain_image_view = swapchain_image_views[i];
				auto& framebuffer = m_framebuffers[i];
				auto attachments = std::array{ swapchain_image_view, depth_image_view };

				framebuffer = device.createFramebuffer(
					vk::FramebufferCreateInfo{}
					.setAttachments(attachments)
					.setRenderPass(render_pass)
					.setWidth(width)
					.setHeight(height)
					.setLayers(1)
				);
			}
		}
		void destroy_framebuffers() {
			vk::Device device = parent::get_device();
			std::ranges::for_each(m_framebuffers,
				[device](auto framebuffer) {
					device.destroyFramebuffer(framebuffer);
				});
		}
		auto get_framebuffers() {
			return m_framebuffers;
		}
	private:
		std::vector<vk::Framebuffer> m_framebuffers;
	};
	template<class T>
	class add_depth_images_views_cube : public T {
	public:
		using parent = T;
		add_depth_images_views_cube() {
			create();
		}
		~add_depth_images_views_cube() {
			destroy();
		}
		void create() {
			vk::Device device = parent::get_device();
			auto images = parent::get_images();
			vk::Format format = parent::get_image_format();

			m_views.resize(images.size());
			std::ranges::transform(
				images,
				m_views.begin(),
				[device, format](auto image) {
					return device.createImageView(
						vk::ImageViewCreateInfo{}
						.setImage(image)
						.setFormat(format)
						.setSubresourceRange(
							vk::ImageSubresourceRange{}
							.setAspectMask(vk::ImageAspectFlagBits::eDepth)
							.setLayerCount(1)
							.setLevelCount(1)
						)
						.setViewType(vk::ImageViewType::e2D)
					);
				}
			);
		}
		void destroy() {
			vk::Device device = parent::get_device();
			std::ranges::for_each(
				m_views,
				[device](auto view) {
					device.destroyImageView(view);
				}
			);
		}
		auto get_images_views() {
			return m_views;
		}
	private:
		std::vector<vk::ImageView> m_views;
	};
	template<class T>
	class add_render_pass_cube : public T {
	public:
		using parent = T;
		add_render_pass_cube() {
			vk::Device device = parent::get_device();
			auto attachments = parent::get_attachments();
			auto dependencies = parent::get_subpass_dependencies();
			auto color_attachment = vk::AttachmentReference{}.setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
			auto depth_attachment = vk::AttachmentReference{}.setAttachment(1).setLayout(vk::ImageLayout::eGeneral);
			auto subpasses = std::array{
				vk::SubpassDescription{}.setColorAttachments(color_attachment).setPDepthStencilAttachment(&depth_attachment)
			};

			m_render_pass = device.createRenderPass(
				vk::RenderPassCreateInfo{}
				.setAttachments(attachments)
				.setDependencies(dependencies)
				.setSubpasses(subpasses)
			);
		}
		~add_render_pass_cube() {
			vk::Device device = parent::get_device();
			device.destroyRenderPass(m_render_pass);
		}
		auto get_render_pass() {
			return m_render_pass;
		}
	private:
		vk::RenderPass m_render_pass;
	};
	template<class T>
	class adjust_draw_rate : public T {
	public:
		using parent = T;
		adjust_draw_rate() {
			m_start_time = std::chrono::steady_clock::now();
		}
		void draw() {
			using namespace std::literals;
			auto duration = std::chrono::steady_clock::now() - m_start_time;
			auto frame_duration = parent::get_frame_index() * 100ms;
			using CT = std::common_type<decltype(duration), decltype(frame_duration)>::type;
			CT d = duration;
			CT fd = frame_duration;
			if (operator<(fd, d)) {
				parent::draw();
			}
		}
	private:
		std::chrono::steady_clock::time_point m_start_time;
	};
	template<class T>
	class add_dynamic_draw : public T {
	public:
		using parent = T;
		add_dynamic_draw() {
			
		}
		void draw() {
			vk::Device device = parent::get_device();
			vk::SwapchainKHR swapchain = parent::get_swapchain();
			vk::Queue queue = parent::get_queue();
			vk::Semaphore acquire_image_semaphore = parent::get_acquire_next_image_semaphore();
			bool need_recreate_surface = false;


			auto [res, index] = device.acquireNextImage2KHR(
				vk::AcquireNextImageInfoKHR{}
				.setSwapchain(swapchain)
				.setSemaphore(acquire_image_semaphore)
				.setTimeout(UINT64_MAX)
				.setDeviceMask(1)
			);
			if (res == vk::Result::eSuboptimalKHR) {
				need_recreate_surface = true;
			}
			else if (res != vk::Result::eSuccess) {
				throw std::runtime_error{ "acquire next image != success" };
			}
			parent::free_acquire_next_image_semaphore(index);

			vk::Fence acquire_next_image_semaphore_fence = parent::get_acquire_next_image_semaphore_fence(index);
			{
				vk::Result res = device.waitForFences(acquire_next_image_semaphore_fence, true, UINT64_MAX);
				if (res != vk::Result::eSuccess) {
					throw std::runtime_error{ "failed to wait fences" };
				}
			}
			device.resetFences(acquire_next_image_semaphore_fence);

			std::vector<void*> upload_memory_ptrs = parent::get_uniform_upload_buffer_memory_ptr_vector();
			void* upload_ptr = upload_memory_ptrs[index];
			memcpy(upload_ptr, &m_frame_index, sizeof(m_frame_index));
			m_frame_index++;
			std::vector<vk::DeviceMemory> upload_memory_vector = parent::get_uniform_upload_buffer_memory_vector();
			vk::DeviceMemory upload_memory = upload_memory_vector[index];
			device.flushMappedMemoryRanges(
				vk::MappedMemoryRange{}
				.setMemory(upload_memory)
				.setOffset(0)
				.setSize(vk::WholeSize)
			);

			void* vertex_ptr = parent::get_vertex_buffer_memory_ptr_vector()[index];
			auto vertices = std::array{
	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f, 1.0f,
	-1.0f, 1.0f, -1.0f,
	-1.0f, 1.0f, 1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, 1.0f,
	1.0f, 1.0f, -1.0f,
	1.0f, 1.0f, 1.0f,
			};
			std::ranges::for_each(vertices,
				[frame_index=m_frame_index](float& v) {
					v *= 1/60.0 * (frame_index % 60);
				});
			memcpy(vertex_ptr, vertices.data(), vertices.size() * sizeof(vertices[0]));
			device.flushMappedMemoryRanges(
				vk::MappedMemoryRange{}
				.setMemory(parent::get_vertex_buffer_memory_vector()[index])
				.setOffset(0)
				.setSize(vk::WholeSize)
			);

			vk::Semaphore draw_image_semaphore = parent::get_draw_image_semaphore(index);
			vk::CommandBuffer buffer = parent::get_swapchain_command_buffer(index);
			vk::PipelineStageFlags wait_stage_mask{ vk::PipelineStageFlagBits::eTopOfPipe };
			queue.submit(
				vk::SubmitInfo{}
				.setCommandBuffers(buffer)
				.setWaitSemaphores(
					acquire_image_semaphore
				)
				.setWaitDstStageMask(wait_stage_mask)
				.setSignalSemaphores(
					draw_image_semaphore
				),
				acquire_next_image_semaphore_fence
			);
			try {
				auto res = queue.presentKHR(
					vk::PresentInfoKHR{}
					.setImageIndices(index)
					.setSwapchains(swapchain)
					.setWaitSemaphores(draw_image_semaphore)
				);
				if (res == vk::Result::eSuboptimalKHR) {
					need_recreate_surface = true;
				}
				else if (res != vk::Result::eSuccess) {
					throw std::runtime_error{ "present return != success" };
				}
			}
			catch (vk::OutOfDateKHRError e) {
				need_recreate_surface = true;
			}
			if (need_recreate_surface) {
				queue.waitIdle();
				parent::recreate_surface();
			}
		}
		~add_dynamic_draw() {
			vk::Device device = parent::get_device();
			vk::Queue queue = parent::get_queue();
			queue.waitIdle();
		}
		auto get_frame_index() {
			return m_frame_index;
		}
	private:
		uint64_t m_frame_index = 0;
	};


	using draw_cube_app =
		add_window_loop <
		jump_draw_if_window_minimized <
		adjust_draw_rate<
		add_dynamic_draw <
		add_acquire_next_image_semaphores <
		add_acquire_next_image_semaphore_fences <
		add_draw_semaphores <
		add_recreate_surface_for <
		record_swapchain_command_buffers_cube <
		add_get_format_clear_color_value_type <
		add_recreate_surface_for <
		add_swapchain_command_buffers <
		add_command_pool <
		add_queue <
		write_descriptor_set<
		add_nonfree_descriptor_set<
		add_descriptor_pool<
		add_buffer_memory_with_data_copy<
		rename_buffer_to_index_buffer<
		add_buffer_as_member<
		set_buffer_usage<vk::BufferUsageFlagBits::eIndexBuffer,
		add_cube_index_buffer_data<
		rename_buffer_vector_to_uniform_upload_buffer_vector <
		rename_buffer_memory_vector_to_uniform_upload_buffer_memory_vector<
		rename_buffer_memory_ptr_vector_to_uniform_upload_buffer_memory_ptr_vector<
		map_buffer_memory_vector<
		add_buffer_memory_vector<
		set_buffer_memory_properties < vk::MemoryPropertyFlagBits::eHostVisible,
		add_buffer_vector<
		set_vector_size_to_swapchain_image_count<
		set_buffer_usage<vk::BufferUsageFlagBits::eTransferSrc,
		rename_buffer_vector_to_uniform_buffer_vector<
		add_buffer_memory_vector<
		set_buffer_memory_properties<vk::MemoryPropertyFlagBits::eDeviceLocal,
		add_buffer_vector<
		set_vector_size_to_swapchain_image_count <
		add_buffer_usage<vk::BufferUsageFlagBits::eTransferDst,
		add_buffer_usage<vk::BufferUsageFlagBits::eUniformBuffer,
		empty_buffer_usage<
		set_buffer_size<sizeof(uint64_t),
		rename_buffer_memory_ptr_vector_to_vertex_buffer_memory_ptr_vector<
		map_buffer_memory_vector<
		rename_buffer_memory_vector_to_vertex_buffer_memory_vector<
		add_buffer_memory_vector <
		set_buffer_memory_properties < vk::MemoryPropertyFlagBits::eHostVisible,
		rename_buffer_vector_to_vertex_buffer_vector<
		add_buffer_vector <
		set_vector_size_to_swapchain_image_count <
		set_buffer_usage<vk::BufferUsageFlagBits::eVertexBuffer,
		add_cube_vertex_buffer_data <
		add_recreate_surface_for_pipeline <
		add_graphics_pipeline <
		add_pipeline_vertex_input_state <
		add_vertex_binding_description <
		add_empty_binding_descriptions <
		add_vertex_attribute_description <
		set_vertex_input_attribute_format<vk::Format::eR32G32B32Sfloat,
		add_empty_vertex_attribute_descriptions <
		set_binding < 0,
		set_stride < sizeof(float) * 3,
		set_input_rate < vk::VertexInputRate::eVertex,
		set_subpass < 0,
		add_recreate_surface_for_framebuffers <
		add_framebuffers_cube <
		add_render_pass_cube <
		add_subpasses <
		add_subpass_dependency <
		add_empty_subpass_dependencies <
		add_depth_attachment<
		add_attachment <
		add_empty_attachments <
		add_pipeline_viewport_state <
		add_scissor_equal_surface_rect <
		add_empty_scissors <
		add_viewport_equal_swapchain_image_rect <
		add_empty_viewports <
		set_tessellation_patch_control_point_count < 1,
		add_pipeline_stage_to_stages <
		add_pipeline_stage <
		set_shader_stage < vk::ShaderStageFlagBits::eVertex,
		add_shader_module <
		add_spirv_code <
		adapte_map_file_to_spirv_code <
		map_file_mapping <
		cache_file_size <
		add_file_mapping <
		add_file <
		add_cube_vertex_shader_path <
		add_pipeline_stage_to_stages <
		add_pipeline_stage <
		set_shader_stage < vk::ShaderStageFlagBits::eFragment,
		set_shader_entry_name_with_main <
		add_shader_module <
		add_spirv_code <
		adapte_map_file_to_spirv_code <
		map_file_mapping <
		cache_file_size <
		add_file_mapping <
		add_file <
		add_cube_fragment_shader_path <
		add_empty_pipeline_stages <
		add_pipeline_layout <
		add_single_descriptor_set_layout<
		add_descriptor_set_layout<
		add_cube_descriptor_set_layout_binding<
		set_pipeline_rasterization_polygon_mode < vk::PolygonMode::eFill,
		disable_pipeline_multisample <
		set_pipeline_input_topology < vk::PrimitiveTopology::eTriangleList,
		disable_pipeline_dynamic <
		enable_pipeline_depth_test <
		add_pipeline_color_blend_state_create_info <
		disable_pipeline_attachment_color_blend < 0, // disable index 0 attachment
		add_pipeline_color_blend_attachment_states < 1, // 1 attachment
		rename_images_views_to_depth_images_views<
		add_recreate_surface_for<
		add_depth_images_views_cube<
		add_recreate_surface_for_images_memories<
		add_images_memories<
		add_image_memory_property<vk::MemoryPropertyFlagBits::eDeviceLocal,
		add_empty_image_memory_properties<
		add_recreate_surface_for_images<
		add_images<
		add_image_type<vk::ImageType::e2D,
		set_image_tiling<vk::ImageTiling::eOptimal,
		set_image_samples<vk::SampleCountFlagBits::e1,
		add_image_extent_equal_swapchain_image_extent<
		add_image_usage<vk::ImageUsageFlagBits::eDepthStencilAttachment,
		add_empty_image_usages<
		rename_image_format_to_depth_image_format<
		add_image_format<vk::Format::eD32Sfloat,
		add_image_count_equal_swapchain_image_count<
		add_recreate_surface_for_swapchain_images_views <
		add_swapchain_images_views <
		add_recreate_surface_for_swapchain_images <
		add_swapchain_images <
		add_recreate_surface_for_swapchain <
		add_swapchain <
		add_swapchain_image_extent_equal_surface_current_extent <
		add_swapchain_image_format <
		add_device <
		add_swapchain_extension <
		add_empty_extensions <
		add_find_properties <
		cache_physical_device_memory_properties<
		add_recreate_surface_for_cache_surface_capabilites<
		cache_surface_capabilities<
		add_recreate_surface_for<
		test_physical_device_support_surface<
		add_queue_family_index <
		add_physical_device<
		add_recreate_surface<
		vulkan_windows_helper::add_windows_surface<
		add_instance<
		add_win32_surface_extension<
		add_surface_extension<
		add_empty_extensions<
		add_window<
		adjust_window_resolution<
		set_window_resolution<151, 151,
		set_window_style<WS_OVERLAPPEDWINDOW,
		add_window_class<
		add_window_process<
		empty_class
		>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> >
		>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> >
		>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		;


int main() {
	try {
		using namespace windows_helper;
		using namespace vulkan_hpp_helper;
		auto app = draw_cube_app{};
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
	return 0;
}