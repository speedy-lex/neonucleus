#include "../neonucleus.h"

void nn_fs_destroy(void *_, nn_component *component, nn_filesystem *fs) {
    if(!nn_decRef(&fs->refc)) return;

    if(fs->deinit != NULL) {
        fs->deinit(component, fs->userdata);
    }
}

nn_bool_t nn_fs_illegalPath(const char *path) {
    // absolute disaster
    const char *illegal = "\"\\:*?<>|";

    for(nn_size_t i = 0; illegal[i] != '\0'; i++) {
        if(nn_strchr(path, illegal[i]) != NULL) return true;
    }
    return false;
}

nn_filesystemControl nn_fs_getControl(nn_component *component, nn_filesystem *fs) {
    return fs->control(component, fs->userdata);
}

void nn_fs_readCost(nn_filesystem *fs, nn_size_t bytes, nn_component *component) {
    nn_filesystemControl control = nn_fs_getControl(component, fs);
    nn_computer *computer = nn_getComputerOfComponent(component);

    nn_simulateBufferedIndirect(component, bytes, control.readBytesPerTick);
    nn_removeEnergy(computer, control.readEnergyPerByte * bytes);
    nn_addHeat(computer, control.readHeatPerByte * bytes);
}

void nn_fs_writeCost(nn_filesystem *fs, nn_size_t bytes, nn_component *component) {
    nn_filesystemControl control = nn_fs_getControl(component, fs);
    nn_computer *computer = nn_getComputerOfComponent(component);

    nn_simulateBufferedIndirect(component, bytes, control.writeBytesPerTick);
    nn_removeEnergy(computer, control.writeEnergyPerByte * bytes);
    nn_addHeat(computer, control.writeHeatPerByte * bytes);
}

void nn_fs_removeCost(nn_filesystem *fs, nn_size_t count, nn_component *component) {
    nn_filesystemControl control = nn_fs_getControl(component, fs);
    nn_computer *computer = nn_getComputerOfComponent(component);

    nn_simulateBufferedIndirect(component, count, control.removeFilesPerTick);
    nn_removeEnergy(computer, control.removeEnergy * count);
    nn_addHeat(computer, control.removeHeat * count);
}

void nn_fs_createCost(nn_filesystem *fs, nn_size_t count, nn_component *component) {
    nn_filesystemControl control = nn_fs_getControl(component, fs);
    nn_computer *computer = nn_getComputerOfComponent(component);

    nn_simulateBufferedIndirect(component, count, control.createFilesPerTick);
    nn_removeEnergy(computer, control.createEnergy * count);
    nn_addHeat(computer, control.createHeat * count);
}

void nn_fs_getLabel(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    char buf[NN_LABEL_SIZE];
    nn_size_t l = NN_LABEL_SIZE;
    fs->getLabel(component, fs->userdata, buf, &l);
    if(l == 0) {
        nn_return(computer, nn_values_nil());
    } else {
        nn_return_string(computer, buf, l);
    }

    nn_fs_readCost(fs, l, component);
}

void nn_fs_setLabel(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_size_t l = 0;
    nn_value label = nn_getArgument(computer, 0);
    const char *buf = nn_toString(label, &l);
    if(buf == NULL) {
        nn_setCError(computer, "bad label (string expected)");
        return;
    }
    l = fs->setLabel(component, fs->userdata, buf, l);
    nn_return_string(computer, buf, l);

    nn_fs_writeCost(fs, l, component);
}

void nn_fs_spaceUsed(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_size_t space = fs->spaceUsed(component, fs->userdata);
    nn_return(computer, nn_values_integer(space));
}

void nn_fs_spaceTotal(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_size_t space = fs->spaceUsed(component, fs->userdata);
    nn_return(computer, nn_values_integer(space));
}

void nn_fs_isReadOnly(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_return(computer, nn_values_boolean(fs->isReadOnly(component, fs->userdata)));
}

void nn_fs_size(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_size_t byteSize = fs->size(component, fs->userdata, path);

    nn_return(computer, nn_values_integer(byteSize));
}

void nn_fs_remove(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_return(computer, nn_values_boolean(fs->remove(component, fs->userdata, path)));

    nn_fs_removeCost(fs, 1, component);
}

void nn_fs_lastModified(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_size_t t = fs->lastModified(component, fs->userdata, path);

    // OpenOS does BULLSHIT with this thing, dividing it by 1000 and expecting it to be
    // fucking usable as a date, meaning it needs to be an int.
    // Because of that, we ensure it is divisible by 1000
    t -= t % 1000;

    nn_return(computer, nn_values_integer(t));
}

void nn_fs_rename(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value fromValue = nn_getArgument(computer, 0);
    const char *from = nn_toCString(fromValue);
    if(from == NULL) {
        nn_setCError(computer, "bad path #1 (string expected)");
        return;
    }
    if(nn_fs_illegalPath(from)) {
        nn_setCError(computer, "bad path #1 (illegal path)");
        return;
    }
    
    nn_value toValue = nn_getArgument(computer, 0);
    const char *to = nn_toCString(toValue);
    if(to == NULL) {
        nn_setCError(computer, "bad path #2 (string expected)");
        return;
    }
    if(nn_fs_illegalPath(to)) {
        nn_setCError(computer, "bad path #2 (illegal path)");
        return;
    }

    nn_size_t movedCount = fs->rename(component, fs->userdata, from, to);
    nn_return(computer, nn_values_boolean(movedCount > 0));
   
    nn_fs_removeCost(fs, movedCount, component);
    nn_fs_createCost(fs, movedCount, component);
}

void nn_fs_exists(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_return(computer, nn_values_boolean(fs->exists(component, fs->userdata, path)));
}

void nn_fs_isDirectory(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_return(computer, nn_values_boolean(fs->isDirectory(component, fs->userdata, path)));
}

void nn_fs_makeDirectory(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_return(computer, nn_values_boolean(fs->makeDirectory(component, fs->userdata, path)));

    nn_fs_createCost(fs, 1, component);
}

void nn_fs_list(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }
    
    nn_Alloc *alloc = nn_getAllocator(nn_getUniverse(computer));

    nn_size_t fileCount = 0;
    char **files = fs->list(alloc, component, fs->userdata, path, &fileCount);

    if(files != NULL) {
        // operation succeeded
        nn_value arr = nn_values_array(alloc, fileCount);
        for(nn_size_t i = 0; i < fileCount; i++) {
            nn_values_set(arr, i, nn_values_string(alloc, files[i], nn_strlen(files[i])));
            nn_deallocStr(alloc, files[i]);
        }
        nn_dealloc(alloc, files, sizeof(char *) * fileCount);
        nn_return(computer, arr);
    }
}

void nn_fs_open(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value pathValue = nn_getArgument(computer, 0);
    const char *path = nn_toCString(pathValue);
    if(path == NULL) {
        nn_setCError(computer, "bad path (string expected)");
        return;
    }
    if(nn_fs_illegalPath(path)) {
        nn_setCError(computer, "bad path (illegal path)");
        return;
    }

    nn_value modeValue = nn_getArgument(computer, 1);
    const char *mode = nn_toCString(modeValue);
    if(mode == NULL) {
        mode = "r";
    }

    // technically wrongfully 
    if(!fs->exists(component, fs->userdata, path)) {
        nn_fs_createCost(fs, 1, component);
    }

    nn_size_t fd = fs->open(component, fs->userdata, path, mode);
    nn_return(computer, nn_values_integer(fd));
}

void nn_fs_close(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value fdValue = nn_getArgument(computer, 0);
    nn_size_t fd = nn_toInt(fdValue);

    nn_bool_t closed = fs->close(component, fs->userdata, fd);
    nn_return(computer, nn_values_boolean(closed));
}

void nn_fs_write(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value fdValue = nn_getArgument(computer, 0);
    nn_size_t fd = nn_toInt(fdValue);

    // size_t spaceRemaining = fs->spaceTotal(component, fs->userdata) - fs->spaceUsed(component, fs->userdata);

    nn_value bufferValue = nn_getArgument(computer, 1);
    nn_size_t len = 0;
    const char *buf = nn_toString(bufferValue, &len);
    if(buf == NULL) {
        nn_setCError(computer, "bad buffer (string expected)");
        return;
    }

    nn_bool_t closed = fs->write(component, fs->userdata, fd, buf, len);
    nn_return(computer, nn_values_boolean(closed));

    nn_fs_writeCost(fs, len, component);
    nn_return_boolean(computer, true);
}

void nn_fs_read(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_value fdValue = nn_getArgument(computer, 0);
    int fd = nn_toInt(fdValue);

    nn_value lenValue = nn_getArgument(computer, 1);
    double len = nn_toNumber(lenValue);
    nn_size_t capacity = fs->spaceTotal(component, fs->userdata);
    if(len > capacity) len = capacity;
    nn_size_t byteLen = len;

    nn_Alloc *alloc = nn_getAllocator(nn_getUniverse(computer));
    char *buf = nn_alloc(alloc, byteLen);
    if(buf == NULL) {
        nn_setCError(computer, "out of memory");
        return;
    }

    nn_size_t readLen = fs->read(component, fs->userdata, fd, buf, byteLen);
    if(readLen > 0) {
        // Nothing read means EoF.
        nn_return_string(computer, buf, readLen);
    }
    nn_dealloc(alloc, buf, byteLen);

    nn_fs_readCost(fs, len, component);
}

nn_bool_t nn_fs_validWhence(const char *s) {
    return
        nn_strcmp(s, "set") == 0 ||
        nn_strcmp(s, "cur") == 0 ||
        nn_strcmp(s, "end") == 0;
}

void nn_fs_seek(nn_filesystem *fs, void *_, nn_component *component, nn_computer *computer) {
    nn_size_t fd = nn_toInt(nn_getArgument(computer, 0));

    const char *whence = nn_toCString(nn_getArgument(computer, 1));

    int off = nn_toInt(nn_getArgument(computer, 2));

    if(whence == NULL) {
        nn_setCError(computer, "bad whence (string expected)");
        return;
    }
    
    if(!nn_fs_validWhence(whence)) {
        nn_setCError(computer, "bad whence");
        return;
    }

    // size_t capacity = fs->spaceTotal(component, fs->userdata);
    int moved = 0;

    nn_size_t pos = fs->seek(component, fs->userdata, fd, whence, off, &moved);
    if(moved < 0) moved = -moved;

    nn_return_integer(computer, pos);
}

void nn_loadFilesystemTable(nn_universe *universe) {
    nn_componentTable *fsTable = nn_newComponentTable(nn_getAllocator(universe), "filesystem", NULL, NULL, (void *)nn_fs_destroy);
    nn_storeUserdata(universe, "NN:FILESYSTEM", fsTable);

    nn_defineMethod(fsTable, "getLabel", true, (void *)nn_fs_getLabel, NULL, "getLabel(): string - Returns the label of the filesystem.");
    nn_defineMethod(fsTable, "setLabel", true, (void *)nn_fs_setLabel, NULL, "setLabel(label: string): string - Sets a new label for the filesystem and returns the new label of the filesystem, which may have been truncated.");
    nn_defineMethod(fsTable, "spaceUsed", true, (void *)nn_fs_spaceUsed, NULL, "spaceUsed(): integer - Returns the amounts of bytes used.");
    nn_defineMethod(fsTable, "spaceTotal", true, (void *)nn_fs_spaceTotal, NULL, "spaceTotal(): integer - Returns the capacity of the filesystem.");
    nn_defineMethod(fsTable, "isReadOnly", true, (void *)nn_fs_isReadOnly, NULL, "isReadOnly(): boolean - Returns whether the filesystem is in read-only mode.");
    nn_defineMethod(fsTable, "size", true, (void *)nn_fs_size, NULL, "size(path: string): integer - Gets the size, in bytes, of a file.");
    nn_defineMethod(fsTable, "remove", true, (void *)nn_fs_remove, NULL, "remove(path: string): boolean - Removes a file. Returns whether the operation succeeded.");
    nn_defineMethod(fsTable, "lastModified", true, (void *)nn_fs_lastModified, NULL, "remove(path: string): boolean - Removes a file. Returns whether the operation succeeded.");
    nn_defineMethod(fsTable, "rename", true, (void *)nn_fs_rename, NULL, "rename(from: string, to: string): boolean - Moves files from one path to another.");
    nn_defineMethod(fsTable, "exists", true, (void *)nn_fs_exists, NULL, "exists(path: string): boolean - Checks whether a file exists.");
    nn_defineMethod(fsTable, "isDirectory", true, (void *)nn_fs_isDirectory, NULL, "isDirectory(path: string): boolean - Returns whether a file is actually a directory.");
    nn_defineMethod(fsTable, "makeDirectory", true, (void *)nn_fs_makeDirectory, NULL, "makeDirectory(path: string): boolean - Creates a new directory at the given path. Returns whether it succeeded.");
    nn_defineMethod(fsTable, "list", true, (void *)nn_fs_list, NULL, "list(path: string): string[] - Returns a list of file paths. Directories will have a / after them");
    nn_defineMethod(fsTable, "open", true, (void *)nn_fs_open, NULL, "open(path: string[, mode: string = \"r\"]): integer - Opens a file, may create it.");
    nn_defineMethod(fsTable, "close", true, (void *)nn_fs_close, NULL, "close(fd: integer): boolean - Closes a file.");
    nn_defineMethod(fsTable, "write", true, (void *)nn_fs_write, NULL, "write(fd: integer, data: string): boolean - Writes data to a file.");
    nn_defineMethod(fsTable, "read", true, (void *)nn_fs_read, NULL, "read(fd: integer, len: number): string - Reads bytes from a file. Infinity is a valid length, in which case it reads as much as possible.");
    nn_defineMethod(fsTable, "seek", true, (void *)nn_fs_seek, NULL, "seek(fd: integer, whence: string, offset: integer): integer - Seeks a file. Returns the new position. Valid whences are set, cur and end.");
}

nn_component *nn_addFileSystem(nn_computer *computer, nn_address address, int slot, nn_filesystem *filesystem) {
    nn_componentTable *fsTable = nn_queryUserdata(nn_getUniverse(computer), "NN:FILESYSTEM");
    return nn_newComponent(computer, address, slot, fsTable, filesystem);
}
