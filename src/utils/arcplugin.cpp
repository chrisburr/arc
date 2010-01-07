#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include <glibmm/module.h>

#include <arc/loader/Plugin.h>
#include <arc/StringConv.h>

static std::string encode_for_var(uint32_t v) {
  return "\"" + Arc::tostring(v) + "\"";
}

static std::string encode_for_var(const char* str) {
    std::string stro = "\"";
    stro += str;
    std::string::size_type p = 1;
    for(;;++p) {
        p = stro.find_first_of("\"\\",p);
        if(p == std::string::npos) break;
        stro.insert(p, "\\");
        ++p;
    }
    stro += "\"";
    return stro;
}

static std::string replace_file_suffix(const std::string& path) {
  std::string newpath = path;
  std::string::size_type name_p = newpath.rfind(G_DIR_SEPARATOR_S);
  if(name_p == std::string::npos) {
    name_p = 0;
  } else {
    ++name_p;
  }
  std::string::size_type suffix_p = newpath.find('.',name_p);
  if(suffix_p != std::string::npos) {
    newpath.resize(suffix_p);
  }
  newpath += ".apd";
  return newpath;
}

int main(int argc, char **argv)
{
    bool create_apd = false;
    if (argc > 1) {
      if (strcmp(argv[1],"-c") == 0) {
        create_apd = true;
        --argc; ++argv;
      }
    }
   
    if (argc < 2) {
        std::cerr << "Invalid arguments" << std::endl;        
        return -1;
    }

    std::string plugin_filename = argv[1];
    std::string descriptor_filename = replace_file_suffix(plugin_filename);

    Glib::ModuleFlags flags = Glib::ModuleFlags(0);
    flags|=Glib::MODULE_BIND_LAZY;
    Glib::Module *module = new Glib::Module(plugin_filename,flags);
    if ((!module) || (!(*module))) {
        std::cerr << "Failed to load module " << plugin_filename << ": " << Glib::Module::get_last_error() << std::endl;
        return -1;
    }

    std::cout << "Loaded module " << plugin_filename << std::endl;

    void *ptr = NULL;
    if(!module->get_symbol(PLUGINS_TABLE_SYMB,ptr)) {
        std::cerr << "Module " << plugin_filename << " is not an ARC plugin: " << Glib::Module::get_last_error() << std::endl;
        delete module;
        return -1;
    };

    Arc::PluginDescriptor* desc = (Arc::PluginDescriptor*)ptr;

    std::ofstream apd;
    if(create_apd) {
      apd.open(descriptor_filename.c_str());
      if(!apd) {
        std::cerr << "Failed to create descriptor file " << descriptor_filename << std::endl;
        return -1;
      };
    };

    for(;desc;++desc) {
        if(desc->name == NULL) break;
        if(desc->kind == NULL) break;
        if(desc->instance == NULL) break;
        if(create_apd) {
            apd << "name=" << encode_for_var(desc->name) << std::endl;
            apd << "kind=" << encode_for_var(desc->kind) << std::endl;
            apd << "version=" << encode_for_var(desc->version) << std::endl;
            apd << std::endl; // end of description mark
        } else {
            std::cout << "name: " << desc->name << std::endl;
            std::cout << "kind: " << desc->kind << std::endl;
            std::cout << "version: " << desc->version << std::endl;
            std::cout << std::endl;
        };
    };

    if(create_apd) {
        apd.close();
        std::cout << "Created descriptor " << descriptor_filename << std::endl;
    };

    delete module;

    return 0;
}
