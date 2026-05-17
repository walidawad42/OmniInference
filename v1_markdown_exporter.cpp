#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <fstream>

class MarkdownExporter {
public:
    // Strict LaTeX wrapping for Google Docs compatibility
    static std::string ProcessForExport(const std::string& raw_output) {
        std::stringstream ss(raw_output);
        std::string line;
        std::string processed_output;
        
        while (std::getline(ss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

            // Detection Logic: Is this a robotics equation?
            bool is_equation = DetectEquation(line);

            if (is_equation) {
                // STRICT WRAPPING PROTOCOL
                // Ensure no single $ exists, only double $$                 // Strip existing single markers if present
                line = std::regex_replace(line, std::regex(R"(\$)"), "");
                
                // Wrap in $$                 processed_output += "$$ " + line + " $$\n";
            } else {
                // Standard text, preserve markdown formatting
                processed_output += line + "\n";
            }
        }
        return processed_output;
    }

    // Save to file
    static void ExportToFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "# Omni-Inference Report\n\n";
            file << ProcessForExport(content);
            file.close();
        }
    }

private:
    static bool DetectEquation(const std::string& line) {
        // Heuristic 1: Contains common LaTeX keywords
        if (line.find("\\frac") != std::string::npos || 
            line.find("\\sum") != std::string::npos ||
            line.find("\\int") != std::string::npos ||
            line.find("\\alpha") != std::string::npos ||
            line.find("\\beta") != std::string::npos) {
            return true;
        }

        // Heuristic 2: Robotics Kinematics/Dynamics patterns (e.g., x = y + z)
        // We look for assignment operators often used in robotics descriptions
        if (std::regex_search(line, std::regex(R"([a-zA-Z]+\s*=\s*[a-zA-Z0-9\+\-\*\/\(\)]+)"))) {
            // Exclude variable definitions (e.g., "temp = 5")
            // Include if it looks math-y (contains subscripts or greek names)
            if (line.find("_") != std::string::npos || line.find("^") != std::string::npos) {
                return true;
            }
        }

        // Heuristic 3: Matrix/Vector notation
        if (line.find("[") != std::string::npos && line.find("]") != std::string::npos) {
             return true;
        }

        return false;
    }
};