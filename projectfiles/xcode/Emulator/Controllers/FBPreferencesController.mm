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

#import "FBPreferencesController.h"

#import "NSWindowController+Core.h"
#import "AKKeyCaptureView.h"

@interface FBPreferencesController()

- (void) resetDipSwitches:(NSArray *) switches;
- (void) resetInputs;

@end

@implementation FBPreferencesController
{
    NSArray<FBDipSetting *> *dipSwitches;
    NSArray<FBInputMapping *> *inputs;
    AKKeyCaptureView *keyCaptureView;
    BOOL inputsDirty;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Preferences"]) {
    }

    return self;
}

- (void) awakeFromNib
{
    [self.runloop addObserver:self];
}

- (void) dealloc
{
    [self.runloop removeObserver:self];
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    toolbar.selectedItemIdentifier = [NSUserDefaults.standardUserDefaults objectForKey:@"selectedPreferencesTab"];
    [self resetDipSwitches:[self.runloop dipSwitches]];
    [self resetInputs];
}

- (id) windowWillReturnFieldEditor:(NSWindow *) sender
                          toObject:(id) anObject
{
    if (anObject == inputTableView) {
        if (!keyCaptureView)
            keyCaptureView = [AKKeyCaptureView new];
        return keyCaptureView;
    }

    return nil;
}

#pragma mark - NSWindowDelegate

- (void) windowDidResignKey:(NSNotification *) notification
{
    // FIXME!!
    NSLog(@"Save input here...");
}

#pragma mark - Actions

- (void) tabChanged:(id) sender
{
    NSToolbarItem *item = (NSToolbarItem *) sender;
    NSString *tabId = item.itemIdentifier;

    toolbar.selectedItemIdentifier = tabId;
    [NSUserDefaults.standardUserDefaults setObject:tabId
                                            forKey:@"selectedPreferencesTab"];
}

- (void) showPreviousTab:(id) sender
{
    __block int selected = -1;
    NSArray<NSToolbarItem *> *items = toolbar.visibleItems;
    [items enumerateObjectsUsingBlock:^(NSToolbarItem *item, NSUInteger idx, BOOL *stop) {
        if ([item.itemIdentifier isEqualToString:toolbar.selectedItemIdentifier]) {
            selected = (int) idx;
            *stop = YES;
        }
    }];

    if (selected < 0 || --selected < 0)
        selected = items.count - 1;

    NSString *nextId = items[selected].itemIdentifier;
    toolbar.selectedItemIdentifier = nextId;

    [NSUserDefaults.standardUserDefaults setObject:nextId
                                            forKey:@"selectedPreferencesTab"];
}

- (void) showNextTab:(id) sender
{
    __block int selected = -1;
    NSArray<NSToolbarItem *> *items = toolbar.visibleItems;
    [items enumerateObjectsUsingBlock:^(NSToolbarItem *item, NSUInteger idx, BOOL *stop) {
        if ([item.itemIdentifier isEqualToString:toolbar.selectedItemIdentifier]) {
            selected = (int) idx;
            *stop = YES;
        }
    }];

    if (selected < 0 || ++selected >= items.count)
        selected = 0;

    NSString *nextId = items[selected].itemIdentifier;
    toolbar.selectedItemIdentifier = nextId;

    [NSUserDefaults.standardUserDefaults setObject:nextId
                                            forKey:@"selectedPreferencesTab"];
}

- (void) restoreDipToDefault:(id) sender
{
    for (FBDipSetting *sw in dipSwitches) {
        if (sw.selectedIndex != sw.defaultIndex) {
            sw.selectedIndex = sw.defaultIndex;
            [self.runloop applyDip:sw.switches[sw.defaultIndex]];
        }
    }
    [dipswitchTableView reloadData];
}

#pragma mark - NSTabViewDelegate

- (void) tabView:(NSTabView *) tabView
didSelectTabViewItem:(NSTabViewItem *) tabViewItem
{
    // FIXME!!
//    [self sizeWindowToTabContent:tabViewItem.identifier];
}

#pragma mark - NSTableViewDataSource

- (NSInteger) numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == dipswitchTableView)
        return dipSwitches.count;
    else if (tableView == inputTableView)
        return inputs.count;

    return 0;
}

- (id) tableView:(NSTableView *) tableView
objectValueForTableColumn:(NSTableColumn *) tableColumn
             row:(NSInteger) row
{
    if (tableView == dipswitchTableView) {
        FBDipSetting *sw = dipSwitches[row];
        if ([tableColumn.identifier isEqualToString:@"name"]) {
            return sw.name;
        } else if ([tableColumn.identifier isEqualToString:@"value"]) {
            [tableColumn.dataCell removeAllItems];
            NSMutableSet *used = [NSMutableSet new];
            for (FBDipOption *opt in sw.switches) {
                // NSPopupButtonCell discards duplicated titles,
                // so add a count suffix when a dip name is duplicated (e.g.
                // SFIII, 'Region'/'Asia')
                NSString *name = opt.name;
                for (int i = 2;[used containsObject:name]; i++)
                    name = [NSString stringWithFormat:@"%@ (%d)", opt.name, i];

                [used addObject:name];
                [tableColumn.dataCell addItemWithTitle:name];
            }
            return @(sw.selectedIndex);
        }
    } else if (tableView == inputTableView) {
        FBInputMapping *im = inputs[row];
        if ([tableColumn.identifier isEqualToString:@"name"]) {
            return im.name;
        } else if ([tableColumn.identifier isEqualToString:@"button"]) {
            return [AKKeyCaptureView descriptionForKeyCode:im.code];
        }
    }

    return nil;
}

- (void) tableView:(NSTableView *) tableView
    setObjectValue:(id) object
    forTableColumn:(NSTableColumn *) tableColumn
               row:(NSInteger) row
{
    if (tableView == dipswitchTableView) {
        if ([tableColumn.identifier isEqualToString:@"value"]) {
            dipSwitches[row].selectedIndex = [object intValue];
            [self.runloop applyDip:dipSwitches[row].switches[[object intValue]]];
        }
    } else if (tableView == inputTableView) {
        if ([tableColumn.identifier isEqualToString:@"button"]) {
            inputs[row].code = (int) [AKKeyCaptureView keyCodeForDescription:object];
        }
    }
}

#pragma mark - FBMainThreadDelegate

- (void) driverInitDidStart
{
    [self resetDipSwitches:nil];
    inputs = nil;
    inputsDirty = NO;
    inputTableView.enabled = NO;
    [inputTableView reloadData];
}

- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success
{
    [self resetDipSwitches:[self.runloop dipSwitches]];
    [self resetInputs];
}

#pragma mark - Private

- (void) resetDipSwitches:(NSArray *) switches
{
    dipSwitches = switches;
    restoreDipButton.enabled = dipswitchTableView.enabled = dipSwitches.count > 0;
    [dipswitchTableView reloadData];
}

- (void) resetInputs
{
    inputs = [self.input inputs];
    inputTableView.enabled = inputs.count > 0;
    [inputTableView reloadData];
}

@end
