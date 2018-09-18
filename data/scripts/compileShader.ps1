
param (
    [Parameter(Mandatory=$true)][string]$shaderDir = ""
 )

#$VulkanSDKBin = "$Env:VULKAN_SDK\bin" 
$VulkanSDKBin = "..\..\externals\VulkanSDK\bin" 

write-output "Compiling shaders in: $shaderDir using binaries from $VulkanSDKBin. `n"

$GLSLANGexe = "$VulkanSDKBin\glslangValidator.exe"
$SPIRVOPTexe = "$VulkanSDKBin\spirv-opt.exe"

Invoke-Expression "$GLSLANGexe --version"
write-output "`r"
Invoke-Expression "$SPIRVOPTexe --version"
write-output "`r"

$shaderFiles = get-childitem -Recurse -File -Path $shaderDir -Include *.vert,*.frag,*.comp,*.tesc,*.tese,*.geom

#write-output "$shaderFiles"

write-output "Compiling ...."
foreach ($shaderFile in $shaderFiles){

  $CompileOutput = "$shaderFile.dbg.spv"
  $OptimizeOutput = "$shaderFile.spv"

  $shaderFileLastWriteTime = (Get-Item $shaderFile).LastWriteTime
  #write-output $shaderFileLastWriteTime
  
  $CompileOutputLastWriteTime = 0
  if(Test-Path $CompileOutput -PathType Leaf)
  {
    $CompileOutputLastWriteTime = (Get-Item $CompileOutput).LastWriteTime    
    #write-output $CompileOutputLastWriteTime
  }

  $OptimizeOutputLastWriteTime = 0
  if(Test-Path $OptimizeOutput -PathType Leaf)
  {
    $OptimizeOutputLastWriteTime = (Get-Item $OptimizeOutput).LastWriteTime
    #write-output $OptimizeOutputLastWriteTime
  }

  if ($shaderFileLastWriteTime -gt $CompileOutputLastWriteTime -or $shaderFileLastWriteTime -gt $OptimizeOutputLastWriteTime)
  {
    Invoke-Expression "$GLSLANGexe -V $shaderFile -o $CompileOutput"
    Invoke-Expression "$SPIRVOPTexe -O $CompileOutput -o $OptimizeOutput"
  }
}