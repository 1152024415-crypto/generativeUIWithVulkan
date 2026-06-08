import hilog from '@ohos.hilog';
import fs from '@ohos.file.fs';
import { LANDSCAPE_BG_FILES, ICON_CATEGORIES } from '../prompts/DslPrompts';

export class ResourceCopier {
  private static instance: ResourceCopier | null = null;

  static getInstance(): ResourceCopier {
    if (!ResourceCopier.instance) {
      ResourceCopier.instance = new ResourceCopier();
    }
    return ResourceCopier.instance;
  }

  copyResources(context: Object): void {
    const path = getContext(context).filesDir + '/rawfile';
    if (fs.accessSync(path + '/.core_done')) {
      hilog.info(0x0000, 'ResourceCopier', 'Core resources already exist, skipping');
      return;
    }
    if (!fs.accessSync(path)) {
      fs.mkdirSync(path);
    }

    let buffer: Uint8Array;
    let file: fs.File;

    // Shader files
    const shaderFiles = [
      'simple_rect.vert.spv',
      'simple_rect.frag.spv',
      'simple_rounded_rect.vert.spv',
      'simple_rounded_rect.frag.spv',
      'circle.vert.spv',
      'circle.frag.spv',
      'text_vert.vert.spv',
      'text_frag.frag.spv',
      'msdf_text_frag.frag.spv',
      'image_vert.vert.spv',
      'image_frag.frag.spv',
      'model3d_gaussian.vert.spv',
      'model3d_gaussian.frag.spv',
      'gaussian3d.vert.spv',
      'gaussian3d.frag.spv'
    ];
    for (const shaderFile of shaderFiles) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(shaderFile);
        file = fs.openSync(path + '/' + shaderFile, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        hilog.error(0x0000, 'ResourceCopier', `Shader ${shaderFile} failed: ${JSON.stringify(e)}`);
      }
    }

    // Font files
    const fontsDir = path + '/fonts';
    if (!fs.accessSync(fontsDir)) {
      fs.mkdirSync(fontsDir);
    }
    const fontFiles = [
      'fonts/HarmonyOS_Sans_SC.ttf',
      'fonts/msyh.ttc',
      'fonts/msyhbd.ttc',
      'fonts/font_config.json'
    ];
    for (const fontPath of fontFiles) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(fontPath);
        file = fs.openSync(path + '/' + fontPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        // skip
      }
    }

    // MSDF atlas
    const msdfFiles = ['msdf_atlas.png', 'msdf_atlas.json'];
    for (const msdfName of msdfFiles) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(msdfName);
        file = fs.openSync(path + '/' + msdfName, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        // skip
      }
    }

    // Texture files
    const textureFiles = ['1.jpg', '1.png'];
    for (const textureName of textureFiles) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(textureName);
        file = fs.openSync(path + '/' + textureName, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        // skip
      }
    }

    // Landscape background images
    const backgroundsDir = path + '/backgrounds-landscape';
    if (!fs.accessSync(backgroundsDir)) {
      fs.mkdirSync(backgroundsDir);
    }
    let bgCopied = 0;
    for (const bgFile of LANDSCAPE_BG_FILES) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync('backgrounds-landscape/' + bgFile);
        file = fs.openSync(backgroundsDir + '/' + bgFile, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
        bgCopied++;
      } catch (e) {
        // skip
      }
    }
    hilog.info(0x0000, 'ResourceCopier', `Copied ${bgCopied} landscape background images`);

    // LLM config
    try {
      buffer = getContext(context).resourceManager.getRawFileContentSync('llm_config.json');
      file = fs.openSync(path + '/llm_config.json', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
      fs.writeSync(file.fd, buffer.buffer);
      fs.closeSync(file);
    } catch (e) {
      // skip
    }

    // Create marker file
    file = fs.openSync(path + '/.core_done', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
    fs.closeSync(file);
    hilog.info(0x0000, 'ResourceCopier', '=== Core resources copied successfully ===');
  }

  copyAllResources(context: Object): void {
    const path = getContext(context).filesDir + '/rawfile';
    const allDoneMarker = path + '/.all_done_v2';

    // Ensure core resources are copied first
    this.copyResources(context);

    if (fs.accessSync(allDoneMarker)) {
      hilog.info(0x0000, 'ResourceCopier', 'All resources already exist, skipping');
      return;
    }

    let buffer: Uint8Array;
    let file: fs.File;

    // PLY model files
    const plyModelFiles = [
      'cup.ply', 'test_gaussian.ply', 'test_3dgs.ply', 'real_garden.ply',
      'dense_wave_terrain.ply', 'simple_test.ply', 'realistic_terrain.ply', 'truck_colored.ply'
    ];
    for (const plyFileName of plyModelFiles) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(plyFileName);
        file = fs.openSync(path + '/' + plyFileName, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        // skip
      }
    }

    // Asset icons (preserving category directory structure)
    const assetCategories = Object.keys(ICON_CATEGORIES);
    for (const category of assetCategories) {
      try {
        const categoryPath = path + '/' + category;
        if (!fs.accessSync(categoryPath)) {
          fs.mkdirSync(categoryPath);
        }
        const icons = ICON_CATEGORIES[category];
        if (icons) {
          for (const fileName of icons) {
            try {
              buffer = getContext(context).resourceManager.getRawFileContentSync(category + '/' + fileName);
              file = fs.openSync(categoryPath + '/' + fileName, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
              fs.writeSync(file.fd, buffer.buffer);
              fs.closeSync(file);
            } catch (e) {
              // skip
            }
          }
        }
      } catch (e) {
        // skip
      }
    }

    // Debug cases directories and files
    const debugCasesDirs = [path + '/debug_cases', path + '/debug_cases/custom', path + '/debug_cases/a2ui', path + '/debug_cases/custom_v2'];
    for (const dir of debugCasesDirs) {
      if (!fs.accessSync(dir)) {
        fs.mkdirSync(dir);
      }
    }
    const debugCasesSets = [
      'debug_cases/custom/debug_cases.json',
      'debug_cases/a2ui/debug_cases.json',
      'debug_cases/custom_v2/debug_cases.json'
    ];
    for (const dcPath of debugCasesSets) {
      try {
        buffer = getContext(context).resourceManager.getRawFileContentSync(dcPath);
        file = fs.openSync(path + '/' + dcPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, buffer.buffer);
        fs.closeSync(file);
      } catch (e) {
        // skip
      }
    }

    // Create marker file
    file = fs.openSync(allDoneMarker, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
    fs.closeSync(file);
    hilog.info(0x0000, 'ResourceCopier', '=== All resources copied to: ' + path + ' ===');
  }
}

export function copyResourcesIfNeeded(context: Object): void {
  ResourceCopier.getInstance().copyResources(context);
}

export function copyAllResourcesIfNeeded(context: Object): void {
  ResourceCopier.getInstance().copyAllResources(context);
}
