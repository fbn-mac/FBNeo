// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "FBScanner.h"

#include "burner.h"
#include "burnint.h"
#include "driverlist.h"

#pragma mark - FBROMSet

@implementation FBROMSet

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

#pragma mark - NSCoding

- (instancetype) initWithCoder:(NSCoder *) coder
{
    if ((self = [super init]) != nil) {
        _parent = [coder decodeObjectForKey:@"parent"];
        _name = [coder decodeObjectForKey:@"name"];
        _title = [coder decodeObjectForKey:@"title"];
        _tags = [coder decodeObjectForKey:@"tags"];
        _status = (unsigned char) [coder decodeIntForKey:@"status"];
    }

    return self;
}

- (void) encodeWithCoder:(NSCoder *) coder
{
    [coder encodeObject:_parent forKey:@"parent"];
    [coder encodeObject:_name forKey:@"name"];
    [coder encodeObject:_title forKey:@"title"];
    [coder encodeObject:_tags forKey:@"tags"];
    [coder encodeInt:(int) _status forKey:@"status"];
}

@end

#pragma mark - FBScanner

@interface FBScanner()

- (FBROMSet *) newRomSetWithIndex:(int) index;

@end

@implementation FBScanner
{
}

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

#pragma mark - NSThread

- (void) main
{
    if (!_rootPath)
        return;

    snprintf(szAppRomPaths[0], MAX_PATH, "%s/",
             [_rootPath cStringUsingEncoding:NSUTF8StringEncoding]);

    id<FBScannerDelegate> del = _delegate;
    if ([del respondsToSelector:@selector(scanDidStart)])
        dispatch_async(dispatch_get_main_queue(), ^{ [del scanDidStart]; });

    NSMutableArray<FBROMSet *> *romSets = [NSMutableArray new];
    UINT32 originallyActive = nBurnDrvActive;
    for (unsigned int i = 0; i < nBurnDrvCount && !self.cancelled; i++) {
        FBROMSet *set = [self newRomSetWithIndex:i];
        if (!set)
            continue;

        del = _delegate;
        if ([del respondsToSelector:@selector(scanDidProgress:)])
            dispatch_async(dispatch_get_main_queue(), ^{
                [del scanDidProgress:(float)i/nBurnDrvCount];
            });

        nBurnDrvActive = i;
        set.status = BzipOpen(TRUE);
        BzipClose();
        nBurnDrvActive = originallyActive;

        [romSets addObject:set];
    }

    NSArray<FBROMSet *> *doneSets = (!self.isCancelled) ? romSets : nil;
    del = _delegate;
    if ([del respondsToSelector:@selector(scanDidEnd:)])
        dispatch_async(dispatch_get_main_queue(), ^{ [del scanDidEnd:doneSets]; });
}

- (FBROMSet *) newRomSetWithIndex:(int) index
{
    if (index < 0 || index >= nBurnDrvCount)
        return nil;

    const struct BurnDriver *driver = pDriver[index];

    FBROMSet *set = [FBROMSet new];
    set.name = [NSString stringWithCString:driver->szShortName
                                  encoding:NSASCIIStringEncoding];
    if (driver->szFullNameW)
        set.title = [[NSString alloc] initWithBytes:driver->szFullNameW
                                        length:sizeof(wchar_t) * wcslen(driver->szFullNameW)
                                      encoding:NSUTF32LittleEndianStringEncoding];
    else if (driver->szFullNameA)
        set.title = [NSString stringWithCString:driver->szFullNameA
                                       encoding:NSASCIIStringEncoding];
    if (driver->szParent)
        set.parent = [NSString stringWithCString:driver->szParent
                                        encoding:NSASCIIStringEncoding];

    uint32 hwMask = driver->Hardware & HARDWARE_PUBLIC_MASK;

    NSMutableSet *tags = [NSMutableSet new];
    if (hwMask == HARDWARE_CAPCOM_CPS1 ||
        hwMask == HARDWARE_CAPCOM_CPS2 ||
        hwMask == HARDWARE_CAPCOM_CPS3) {
        // Count the number of buttons
        struct BurnInputInfo bii;
        int fireButtonCount = 0;
        if (driver->GetInputInfo)
            for (int i = 0; driver->GetInputInfo(&bii, i) == 0 && fireButtonCount < 6; i++)
                if (strncasecmp(bii.szInfo, "p1 fire ", 8) == 0)
                    fireButtonCount++;

        if (fireButtonCount >= 6)
            [tags addObject:@"buttons:sf"];
    }
    if (hwMask == HARDWARE_SNK_NEOGEO || hwMask == HARDWARE_SNK_NEOCD)
        [tags addObject:@"buttons:neogeo"];

    if (tags.count > 0)
        set.tags = tags;

    return set;
}

@end
