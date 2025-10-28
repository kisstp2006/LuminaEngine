#include "TypeReflector.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "Clang/ClangParser.h"
#include "CodeGeneration/CodeGenerator.h"
#include "EASTL/sort.h"
#include "ReflectionCore/ReflectedProject.h"

#define VS_PROJECT_ID "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"  // VS Project UID



namespace Lumina::Reflection
{
    FTypeReflector::FTypeReflector(const eastl::string& InSolutionPath)
        :Solution(InSolutionPath.c_str())
    {
    }

    bool FTypeReflector::ParseSolution()
    {
        std::ifstream slnFile(Solution.GetPath().c_str());
        if (!slnFile.is_open())
        {
            std::cerr << "Failed to open solution file: " << Solution.GetPath().c_str() << "\n";
            return false;
        }
    
        eastl::vector<eastl::string> ProjectFilePaths;
        std::string CurrentLine;
        CurrentLine.reserve(512);
    
        while (std::getline(slnFile, CurrentLine))
        {
            eastl::string Line(CurrentLine.c_str());
            
            size_t ProjectIdPos = Line.find(VS_PROJECT_ID);
            if (ProjectIdPos == eastl::string::npos)
            {
                continue;
            }

            size_t NameStart = Line.find(" = \"");
            if (NameStart == eastl::string::npos)
            {
                continue;
            }

            NameStart += 4;
            size_t NameEnd = Line.find("\", \"", NameStart);
            if (NameEnd == eastl::string::npos)
            {
                continue;
            }

            eastl::string ProjectName = Line.substr(NameStart, NameEnd - NameStart);
            
            if (ProjectName == "Reflector")
            {
                continue;
            }

            size_t PathStart = NameEnd + 4;
            size_t PathEnd = Line.find("\"", PathStart);
            if (PathEnd == eastl::string::npos)
            {
                continue;
            }

            eastl::string ProjectRelativePath = Line.substr(PathStart, PathEnd - PathStart);
            
            // Convert backslashes to forward slashes for consistency
            eastl::replace(ProjectRelativePath.begin(), ProjectRelativePath.end(), '\\', '/');
            
            eastl::string ProjectPath = Solution.GetParentPath() + "/" + ProjectRelativePath;
            
            if (std::filesystem::exists(ProjectPath.c_str()))
            {
                ProjectFilePaths.push_back(std::move(ProjectPath));
            }
            else
            {
                std::cerr << "Warning: Project file not found: " << ProjectPath.c_str() << "\n";
            }
        }
    
        slnFile.close();
    
        if (ProjectFilePaths.empty())
        {
            std::cerr << "Warning: No valid projects found in solution\n";
            return false;
        }
    
        eastl::sort(ProjectFilePaths.begin(), ProjectFilePaths.end());
    
        // Parse each project
        for (const eastl::string& FilePath : ProjectFilePaths)
        {
            FReflectedProject Project(Solution.GetPath(), FilePath);
            
            if (Project.Parse())
            {
                Solution.AddReflectedProject(std::move(Project));
            }
        }
        
        return true;
    }

    bool FTypeReflector::Clean()
    {
        return false;
    }

    bool FTypeReflector::Build(FClangParser& Parser)
    {
        Parser.ParsingContext.bInitialPass = true;
        
        // Initial Pass. Register types only.
        eastl::vector<FReflectedProject> Projects;
        Solution.GetProjects(Projects);
        
        for (FReflectedProject& Project : Projects)
        {
            Parser.ParsingContext.ReflectionDatabase.AddReflectedProject(Project);
            
            if (!Parser.Parse(Project.SolutionPath, Project.Headers, Project))
            {
                std::cout << "No files to parse in " << Project.Name.c_str() << "\n";
            }
        }

        Parser.ParsingContext.bInitialPass = false;

        // Second Pass. Traverse children and finish building types.
        for (FReflectedProject& Project : Projects)
        {
            if (!Parser.Parse(Project.SolutionPath, Project.Headers, Project))
            {
                std::cout << "No files to parse in " << Project.Name.c_str() << "\n";
            }
        }

        for (FReflectedProject& Project : Projects)
        {
            Parser.Dispose(Project);
        }

        std::cout << "[Reflection] Number of headers reflected: " << Parser.ParsingContext.NumHeadersReflected << "\n";
        std::cout << "[Reflection] Number of translation units looked at: " << GTranslationUnitsVisited << "\n";
        std::cout << "[Reflection] Number of translation units parsed: " << GTranslationUnitsParsed << "\n";

        if (Parser.ParsingContext.HasError())
        {
            std::cout << "\033[31m" << Parser.ParsingContext.ErrorMessage.c_str() << "\033[0m\n";
        }
        
        return true;
    }

    bool FTypeReflector::Generate(FClangParser& Parser)
    {
        WriteGeneratedFiles(Parser);

        return true;
    }

    void FTypeReflector::Bump()
    {
#if 0
        try
        {
            std::filesystem::current_path(Solution.GetParentPath().c_str());

            eastl::string PremakeFile = Solution.GetParentPath() + "/Tools/premake5.exe";
            eastl::string Command = "\"" + PremakeFile + "\" vs2022";

            int result = std::system(Command.c_str());  // NOLINT(concurrency-mt-unsafe)
            if (result == -1)
            {
                std::cerr << "Failed to launch premake (system() error).\n";
            }
            else if (result != 0)
            {
                std::cerr << "Premake failed with exit code " << result << "\n";
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Filesystem error while setting working directory: " << e.what() << "\n";
        }
#endif
    }
    
    bool FTypeReflector::WriteGeneratedFiles(const FClangParser& Parser)
    {
        FCodeGenerator Generator(Solution, Parser.ParsingContext.ReflectionDatabase);

        Generator.GenerateCodeForSolution();
        
        return true;
    }
}
