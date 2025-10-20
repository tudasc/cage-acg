#ifndef CAGE_METACG_HXX
#define CAGE_METACG_HXX

#include <metadata/MetaData.h>
#include <nlohmann/json.hpp>

namespace cage::mcg
{
  struct src_loc
  {
    auto
    operator== (src_loc const& other) const
    {
      return line == other.line && col == other.col;
    }

    size_t line, col;
  };

  inline void
  to_json(nlohmann::json& j, src_loc const& md)
  {
    j = {
      { "line", md.line },
      { "col", md.col },
    };
  }

  inline void
  from_json (nlohmann::json const& j, src_loc& md)
  {
    j.at ("line").get_to (md.line);
    j.at ("col").get_to (md.col);
  }

  struct md_arg_output
  {
    size_t idx {};
    std::vector<size_t> callees {};
    bool by_ref {};
    src_loc loc {};
  };

  inline void
  to_json (nlohmann::json& j, md_arg_output const& md)
  {
    j = {
      { "callees", md.callees },
      { "idx", md.idx },
      { "by_ref", md.by_ref },
      { "loc", md.loc },
    };
  }

  inline void
  from_json (nlohmann::json const& j, md_arg_output& md)
  {
    j.at ("callees").get_to (md.callees);
    j.at ("idx").get_to (md.idx);
    j.at ("by_ref").get_to (md.by_ref);
    j.at ("loc").get_to (md.loc);
  }

  struct md_arg { size_t idx {}; std::vector<md_arg_output> outs {}; };

  inline void
  to_json (nlohmann::json& j, md_arg const& md)
  {
    j = {
      { "idx", md.idx },
      { "outs", md.outs },
    };
  }

  inline void
  from_json (nlohmann::json const& j, md_arg& md)
  {
    j.at ("idx").get_to (md.idx);
    j.at ("outs").get_to (md.outs);
  }

  struct md_local { std::vector<size_t> callees {}; src_loc loc {}; };

  inline void
  to_json (nlohmann::json& j, md_local const& md)
  {
    j = {
      { "callees", md.callees },
      { "loc", md.loc },
    };
  }

  inline void
  from_json (nlohmann::json const& j, md_local& md)
  {
    j.at ("callees").get_to (md.callees);
    j.at ("loc").get_to (md.loc);
  }

  struct md_arg_flow final: metacg::MetaData::Registrar<md_arg_flow>
  {
    static constexpr auto key = "argflow";

    md_arg_flow () = default;
    md_arg_flow (md_arg_flow const&) = default;

    explicit
    md_arg_flow (nlohmann::json const& j, metacg::StrToNodeMapping&)
    {
      j.at ("args").get_to (args);
    }

    [[nodiscard]] nlohmann::json
    toJson (metacg::NodeToStrMapping&) const override
    {
      return {
        { "args", args }
      };
    }

    [[nodiscard]] char const*
    getKey () const override { return key; }

    [[nodiscard]] std::unique_ptr<metacg::MetaData>
    clone() const override { return std::make_unique<md_arg_flow> (*this); }

    void
    applyMapping (metacg::GraphMapping const&) override {}

    void
    merge (
      MetaData const&,
      std::optional<metacg::MergeAction>,
      metacg::GraphMapping const&
    ) override {
      // TODO(e820, mcg): Support merging
    }

    void
    arg (md_arg&& arg) { args.push_back (std::move (arg)); }

    std::vector<md_arg> args {};
  };

  struct md_locals final: metacg::MetaData::Registrar<md_locals>
  {
    static constexpr auto key = "localflow";

    md_locals () = default;
    md_locals (md_locals const&) = default;

    explicit
    md_locals (nlohmann::json const& j, metacg::StrToNodeMapping&)
    {
      j.at ("locals").get_to (locals);
    }

    [[nodiscard]] nlohmann::json
    toJson (metacg::NodeToStrMapping&) const final
    {
      return {
          { "locals", locals }
      };
    }

    [[nodiscard]] char const*
    getKey () const override { return key; }

    [[nodiscard]] std::unique_ptr<MetaData>
    clone() const override { return std::make_unique<md_locals> (*this); }

    void
    applyMapping (metacg::GraphMapping const&) override {}

    void
    merge (
      MetaData const&,
      std::optional<metacg::MergeAction>,
      metacg::GraphMapping const&
    ) override {
      // TODO(e820, mcg): Support merging
    }

    void
    local (md_local&& local) { locals.push_back (std::move (local)); }

    std::vector<md_local> locals {};
  };
} // namespace cage::mcg

#endif // CAGE_METACG_HXX
