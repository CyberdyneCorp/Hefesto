// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The glTF 2.0 writer (design D14, J2): one mesh node per exported layer
// under a root node that holds the model origin, one material per layer,
// float32 positions and normals, uint32 indices; the JSON written by
// cgltf_write under the C locale, the GLB container and the base64 data URI
// assembled here.
//
// Owner: EXPORT (design D3).

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <deque>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <cgltf.h>
#include <cgltf_write.h>

#if defined(__APPLE__)
#include <xlocale.h>
#endif
#include <clocale>
#include <locale.h>

#include "view3d/export/model.hpp"

namespace hefesto::view3d {

namespace {

// sRGB 0..255 to linear (glTF base colors are linear), as float32 constants
// so that no platform's pow() takes part.
constexpr std::array<float, 256> kSrgbToLinear = {
    0.0F, 0.000303526991F, 0.000607053982F, 0.000910580973F, 0.00121410796F, 0.00151763496F, 0.00182116195F,
    0.00212468882F, 0.00242821593F, 0.0027317428F, 0.00303526991F, 0.00334653584F, 0.00367650739F, 0.00402471703F,
    0.00439144205F, 0.00477695325F, 0.00518151652F, 0.00560539169F, 0.00604883302F, 0.00651209056F, 0.00699541019F,
    0.00749903219F, 0.00802319311F, 0.00856812578F, 0.00913405884F, 0.00972121768F, 0.010329823F, 0.0109600937F,
    0.0116122449F, 0.012286488F, 0.0129830325F, 0.0137020834F, 0.0144438436F, 0.0152085144F, 0.0159962941F,
    0.0168073755F, 0.0176419541F, 0.01850022F, 0.0193823613F, 0.0202885624F, 0.0212190095F, 0.0221738853F,
    0.0231533665F, 0.0241576321F, 0.0251868591F, 0.0262412224F, 0.0273208916F, 0.02842604F, 0.0295568351F,
    0.0307134446F, 0.0318960324F, 0.0331047662F, 0.0343398079F, 0.0356013142F, 0.0368894488F, 0.0382043719F,
    0.0395462364F, 0.0409151986F, 0.0423114114F, 0.043735031F, 0.045186203F, 0.0466650873F, 0.0481718257F,
    0.0497065671F, 0.0512694567F, 0.0528606474F, 0.054480277F, 0.0561284907F, 0.0578054301F, 0.0595112368F,
    0.0612460524F, 0.0630100146F, 0.064803265F, 0.0666259378F, 0.0684781671F, 0.0703600943F, 0.0722718537F,
    0.0742135718F, 0.0761853829F, 0.078187421F, 0.0802198201F, 0.0822827071F, 0.0843762085F, 0.0865004584F,
    0.0886555836F, 0.0908417106F, 0.0930589661F, 0.0953074694F, 0.097587347F, 0.0998987257F, 0.102241732F,
    0.104616486F, 0.107023105F, 0.10946171F, 0.111932427F, 0.114435375F, 0.116970666F, 0.119538426F, 0.122138776F,
    0.124771819F, 0.127437681F, 0.130136475F, 0.13286832F, 0.135633335F, 0.138431609F, 0.141263291F, 0.144128472F,
    0.147027269F, 0.149959788F, 0.152926147F, 0.155926466F, 0.158960834F, 0.162029371F, 0.165132195F, 0.168269396F,
    0.171441108F, 0.174647406F, 0.177888423F, 0.18116425F, 0.18447499F, 0.187820777F, 0.191201687F, 0.194617838F,
    0.198069319F, 0.20155625F, 0.205078736F, 0.208636865F, 0.212230757F, 0.215860501F, 0.219526201F, 0.223227963F,
    0.226965874F, 0.230740055F, 0.23455058F, 0.238397568F, 0.242281124F, 0.246201321F, 0.25015828F, 0.254152089F,
    0.258182853F, 0.262250662F, 0.266355604F, 0.270497799F, 0.274677306F, 0.278894275F, 0.283148736F, 0.287440836F,
    0.291770637F, 0.296138257F, 0.300543785F, 0.304987311F, 0.309468925F, 0.313988715F, 0.318546772F, 0.323143214F,
    0.327778101F, 0.332451522F, 0.337163627F, 0.341914415F, 0.346704066F, 0.351532608F, 0.356400132F, 0.361306787F,
    0.366252601F, 0.371237695F, 0.376262128F, 0.38132602F, 0.386429429F, 0.391572475F, 0.396755219F, 0.401977777F,
    0.407240212F, 0.412542611F, 0.417885065F, 0.423267663F, 0.428690493F, 0.434153646F, 0.439657182F, 0.445201188F,
    0.450785786F, 0.456411034F, 0.462076992F, 0.467783809F, 0.473531485F, 0.479320168F, 0.48514995F, 0.491020858F,
    0.496932983F, 0.502886474F, 0.50888133F, 0.514917672F, 0.520995557F, 0.527115107F, 0.533276379F, 0.539479494F,
    0.545724452F, 0.55201143F, 0.558340371F, 0.564711511F, 0.571124852F, 0.577580452F, 0.584078431F, 0.590618849F,
    0.597201765F, 0.603827357F, 0.610495567F, 0.617206573F, 0.623960376F, 0.630757153F, 0.637596846F, 0.644479692F,
    0.651405632F, 0.658374846F, 0.665387273F, 0.672443151F, 0.679542482F, 0.686685324F, 0.693871737F, 0.701101899F,
    0.708375752F, 0.715693474F, 0.723055124F, 0.730460763F, 0.73791039F, 0.745404184F, 0.752942204F, 0.760524511F,
    0.768151164F, 0.775822222F, 0.783537805F, 0.791297913F, 0.799102724F, 0.806952238F, 0.814846575F, 0.822785735F,
    0.830769897F, 0.838799F, 0.846873224F, 0.854992628F, 0.863157213F, 0.871367097F, 0.8796224F, 0.887923121F,
    0.896269381F, 0.904661179F, 0.913098633F, 0.921581864F, 0.930110872F, 0.938685715F, 0.947306514F, 0.955973327F,
    0.964686275F, 0.973445296F, 0.982250571F, 0.991102099F, 1.0F,
};

// Uses the C locale on this thread while it lives (cgltf formats numbers
// with snprintf). The process locale is left alone: other threads may run.
#if defined(__APPLE__) || defined(__linux__)
class CLocaleScope {
 public:
  CLocaleScope() : previous_(uselocale(c_locale())) {}
  ~CLocaleScope() {
    if (previous_ != nullptr) {
      uselocale(previous_);
    }
  }
  CLocaleScope(const CLocaleScope&) = delete;
  CLocaleScope& operator=(const CLocaleScope&) = delete;

 private:
  static locale_t c_locale() {
    static const locale_t locale = newlocale(LC_ALL_MASK, "C", nullptr);
    return locale;
  }
  locale_t previous_;
};
#else
struct CLocaleScope {};  // other platforms keep the process locale (Hefesto never changes it)
#endif

float linear_channel(float srgb) {
  const float clamped = std::clamp(srgb, 0.0F, 1.0F);
  return kSrgbToLinear[static_cast<std::size_t>(std::lround(clamped * 255.0F))];
}

// glTF is +Y up: (x, y, z) with z up is written as (x, z, -y).
std::array<float, 3> y_up(const float* p) {
  return {p[0], p[2], 0.0F - p[1]};
}

struct Section {
  std::size_t offset = 0;
  std::size_t size = 0;
};

struct LayerSections {
  Section positions;
  Section normals;
  Section indices;
  std::array<float, 3> min{};
  std::array<float, 3> max{};
};

class BinBuilder {
 public:
  LayerSections add(const detail::LayerGeometry& g) {
    LayerSections s;
    s.positions = vectors(g.positions, &s.min, &s.max);
    s.normals = vectors(g.normals, nullptr, nullptr);
    s.indices.offset = bytes_.size();
    for (const std::uint32_t index : g.indices) {
      detail::put_u32(bytes_, index);
    }
    s.indices.size = bytes_.size() - s.indices.offset;
    return s;
  }
  [[nodiscard]] std::vector<std::byte>& bytes() { return bytes_; }

 private:
  Section vectors(const std::vector<float>& xyz, std::array<float, 3>* min, std::array<float, 3>* max) {
    Section s{bytes_.size(), 0};
    if (min != nullptr) {
      min->fill(std::numeric_limits<float>::max());
      max->fill(std::numeric_limits<float>::lowest());
    }
    for (std::size_t i = 0; i + 2 < xyz.size(); i += 3) {
      const std::array<float, 3> v = y_up(&xyz[i]);
      for (std::size_t c = 0; c < 3; ++c) {
        detail::put_f32(bytes_, v[c]);
        if (min != nullptr) {
          (*min)[c] = std::min((*min)[c], v[c]);
          (*max)[c] = std::max((*max)[c], v[c]);
        }
      }
    }
    s.size = bytes_.size() - s.offset;
    return s;
  }

  std::vector<std::byte> bytes_;
};

std::string format_number(double value) {
  std::array<char, 64> buffer{};
  const std::to_chars_result r = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  return r.ec == std::errc{} ? std::string(buffer.data(), r.ptr) : std::string{"1"};
}

std::string base64(std::span<const std::byte> bytes) {
  static constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    const std::size_t n = std::min<std::size_t>(3, bytes.size() - i);
    std::uint32_t word = 0;
    for (std::size_t j = 0; j < 3; ++j) {
      word = (word << 8U) | (j < n ? std::to_integer<std::uint32_t>(bytes[i + j]) : 0U);
    }
    for (std::size_t j = 0; j < 4; ++j) {
      out.push_back(j <= n ? kAlphabet[(word >> (18U - (6U * j))) & 0x3FU] : '=');
    }
  }
  return out;
}

// The cgltf document: every array is sized before pointers into it are
// taken, and every string lives in `strings_`.
class Document {
 public:
  Document(const detail::ExportModel& model, BinBuilder& bin) {
    const std::size_t n = model.layers.size();
    accessors_.resize(3 * n);
    views_.resize(3 * n);
    attributes_.resize(2 * n);
    primitives_.resize(n);
    meshes_.resize(n);
    materials_.resize(n);
    nodes_.resize(n + 1);
    children_.resize(n);
    data_.asset.version = text("2.0");
    data_.asset.generator = text("Hefesto");
    data_.asset.extras.data =
        text(R"({"hefesto":{"units":"um","zscale":)" + format_number(model.zscale) + "}}");
    for (std::size_t i = 0; i < n; ++i) {
      add_layer(i, model.layers[i], bin.add(model.geometry[i]), model.geometry[i]);
    }
    set_root(model);
    bin_size_ = bin.bytes().size();
  }

  // The buffer, named by `uri` (nullptr in a GLB), when there is any data.
  void set_buffer(const std::string* uri) {
    if (bin_size_ == 0) {
      return;
    }
    buffer_.size = bin_size_;
    buffer_.uri = uri != nullptr ? text(*uri) : nullptr;
    data_.buffers = &buffer_;
    data_.buffers_count = 1;
    for (cgltf_buffer_view& view : views_) {
      view.buffer = &buffer_;
    }
  }

  [[nodiscard]] std::string json() {
    const CLocaleScope c_locale;
    const cgltf_options options{};
    const cgltf_size size = cgltf_write(&options, nullptr, 0, &data_);
    std::string out(size, '\0');
    cgltf_write(&options, out.data(), out.size(), &data_);
    while (!out.empty() && out.back() == '\0') {
      out.pop_back();
    }
    return out;
  }

 private:
  char* text(std::string value) {
    strings_.push_back(std::move(value));
    return strings_.back().data();
  }

  cgltf_accessor* accessor(std::size_t index, const Section& section, cgltf_buffer_view_type type, std::size_t count) {
    cgltf_buffer_view& view = views_[index];
    view.offset = section.offset;
    view.size = section.size;
    view.type = type;
    cgltf_accessor& a = accessors_[index];
    a.buffer_view = &view;
    a.count = count;
    return &a;
  }

  void add_layer(std::size_t i, const ExportLayer& layer, const LayerSections& s, const detail::LayerGeometry& g) {
    cgltf_accessor* position = accessor(3 * i, s.positions, cgltf_buffer_view_type_vertices, g.vertex_count());
    position->component_type = cgltf_component_type_r_32f;
    position->type = cgltf_type_vec3;
    position->has_min = position->has_max = 1;
    std::copy(s.min.begin(), s.min.end(), position->min);
    std::copy(s.max.begin(), s.max.end(), position->max);
    cgltf_accessor* normal = accessor((3 * i) + 1, s.normals, cgltf_buffer_view_type_vertices, g.vertex_count());
    normal->component_type = cgltf_component_type_r_32f;
    normal->type = cgltf_type_vec3;
    cgltf_accessor* index = accessor((3 * i) + 2, s.indices, cgltf_buffer_view_type_indices, g.indices.size());
    index->component_type = cgltf_component_type_r_32u;
    index->type = cgltf_type_scalar;
    attributes_[2 * i] = cgltf_attribute{text("POSITION"), cgltf_attribute_type_position, 0, position};
    attributes_[(2 * i) + 1] = cgltf_attribute{text("NORMAL"), cgltf_attribute_type_normal, 0, normal};
    set_material(materials_[i], layer);
    cgltf_primitive& primitive = primitives_[i];
    primitive.type = cgltf_primitive_type_triangles;
    primitive.indices = index;
    primitive.material = &materials_[i];
    primitive.attributes = &attributes_[2 * i];
    primitive.attributes_count = 2;
    meshes_[i].name = text(detail::safe_json_text(layer.name));
    meshes_[i].primitives = &primitive;
    meshes_[i].primitives_count = 1;
    nodes_[i + 1].name = meshes_[i].name;
    nodes_[i + 1].mesh = &meshes_[i];
    children_[i] = &nodes_[i + 1];
  }

  void set_material(cgltf_material& material, const ExportLayer& layer) {
    material.name = text(detail::safe_json_text(layer.material));
    material.has_pbr_metallic_roughness = 1;
    cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
    for (std::size_t c = 0; c < 3; ++c) {
      pbr.base_color_factor[c] = linear_channel(layer.srgb[c]);
    }
    pbr.base_color_factor[3] = 1.0F;
    pbr.metallic_factor = 0.0F;
    pbr.roughness_factor = 1.0F;
  }

  void set_root(const detail::ExportModel& model) {
    cgltf_node& root = nodes_[0];
    root.name = text("model");
    root.has_translation = 1;
    root.translation[0] = static_cast<float>(model.origin_x_um);
    root.translation[1] = 0.0F;
    root.translation[2] = static_cast<float>(0.0 - model.origin_y_um);
    root.children = children_.empty() ? nullptr : children_.data();
    root.children_count = children_.size();
    scene_.nodes = &root_pointer_;
    root_pointer_ = &root;
    scene_.nodes_count = 1;
    data_.accessors = accessors_.data();
    data_.accessors_count = accessors_.size();
    data_.buffer_views = views_.data();
    data_.buffer_views_count = views_.size();
    data_.meshes = meshes_.data();
    data_.meshes_count = meshes_.size();
    data_.materials = materials_.data();
    data_.materials_count = materials_.size();
    data_.nodes = nodes_.data();
    data_.nodes_count = nodes_.size();
    data_.scenes = &scene_;
    data_.scenes_count = 1;
    data_.scene = &scene_;
  }

  cgltf_data data_{};
  std::deque<std::string> strings_;  // stable addresses
  std::vector<cgltf_accessor> accessors_;
  std::vector<cgltf_buffer_view> views_;
  std::vector<cgltf_attribute> attributes_;
  std::vector<cgltf_primitive> primitives_;
  std::vector<cgltf_mesh> meshes_;
  std::vector<cgltf_material> materials_;
  std::vector<cgltf_node> nodes_;
  std::vector<cgltf_node*> children_;
  cgltf_node* root_pointer_ = nullptr;
  cgltf_scene scene_{};
  cgltf_buffer buffer_{};
  std::size_t bin_size_ = 0;
};

void append_chunk(std::vector<std::byte>& out, std::uint32_t type, std::span<const std::byte> data, std::byte pad) {
  const std::size_t padded = (data.size() + 3U) & ~std::size_t{3U};
  detail::put_u32(out, static_cast<std::uint32_t>(padded));
  detail::put_u32(out, type);
  out.insert(out.end(), data.begin(), data.end());
  out.insert(out.end(), padded - data.size(), pad);
}

std::vector<std::byte> glb(const std::string& json, std::span<const std::byte> bin) {
  constexpr std::uint32_t kMagic = 0x46546C67U;     // "glTF"
  constexpr std::uint32_t kJsonChunk = 0x4E4F534AU;  // "JSON"
  constexpr std::uint32_t kBinChunk = 0x004E4942U;   // "BIN\0"
  std::vector<std::byte> chunks;
  append_chunk(chunks, kJsonChunk, std::as_bytes(std::span{json.data(), json.size()}), std::byte{' '});
  if (!bin.empty()) {
    append_chunk(chunks, kBinChunk, bin, std::byte{0});
  }
  std::vector<std::byte> out;
  detail::put_u32(out, kMagic);
  detail::put_u32(out, 2);
  detail::put_u32(out, static_cast<std::uint32_t>(12 + chunks.size()));
  out.insert(out.end(), chunks.begin(), chunks.end());
  return out;
}

diag::Status write_text(gds::ByteSink& out, const std::string& text) {
  return out.write(std::as_bytes(std::span{text.data(), text.size()}));
}

}  // namespace

diag::Status write_gltf(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                        gds::ByteSink& out, diag::DiagnosticSink& sink) {
  const detail::ExportModel model = detail::build_export_model(mesh, stack, options, sink);
  BinBuilder bin;
  Document document(model, bin);
  if (options.format == ExportFormat::glb) {
    document.set_buffer(nullptr);
    return out.write(glb(document.json(), bin.bytes()));
  }
  if (options.bin_out != nullptr) {
    document.set_buffer(&options.bin_uri);
    if (diag::Status written = options.bin_out->write(bin.bytes()); !written.ok()) {
      return written;
    }
  } else {
    const std::string uri = "data:application/octet-stream;base64," + base64(bin.bytes());
    document.set_buffer(&uri);
  }
  return write_text(out, document.json());
}

}  // namespace hefesto::view3d
