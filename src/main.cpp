#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/TextGameObject.hpp>
#include <Geode/modify/CustomizeObjectLayer.hpp>
#include <Geode/utils/base64.hpp>

using namespace geode::prelude;

std::pair<std::string, std::string> splitIntoPair(const std::string& str) {
	auto split = utils::string::split(str, ":");
	auto& key = split[0];
	if (split.size() < 2) return {key, ""};
	auto value = str.substr(key.size() + 1);

	return {key, value};
}

class $modify(MyCustomizeObjectLayer, CustomizeObjectLayer) {

	struct Fields {
		bool m_isImageObject;
	};

	bool init(GameObject* object, cocos2d::CCArray* objectArray) {
		if (!CustomizeObjectLayer::init(object, objectArray)) return false;
		setTextBtn();
		return true;
	};

	void setTextBtn() {
		auto fields = m_fields.self();
		if (auto textObject = typeinfo_cast<TextGameObject*>(m_targetObject)) {
			auto pair = splitIntoPair(textObject->m_text);
			if (pair.first == "image") {
				fields->m_isImageObject = true;
				if (!m_textButton) return;
				if (auto spr = m_textButton->getChildByType<ButtonSprite*>(0)) {
					if (spr->m_label) {
						spr->m_label->setString("Image");
					}
				}
			}
		}
	}

	void onSelectMode(cocos2d::CCObject* sender) {
		auto fields = m_fields.self();

		if (sender->getTag() == 3 && fields->m_isImageObject) {
			utils::file::FilePickOptions options;
			options.filters.push_back({"Images", {"*.png", "*.apng", "*.jpg", "*.jpeg", "*.jfif", "*.pjpeg", "*.pjp", "*.webp", "*.gif", "*.bmp", "*.jxl"}});

			utils::file::pick(file::PickMode::OpenFile, options).listen([this](Result<std::filesystem::path>* result) {
				if (!*result) return;
				auto path = result->unwrap();
				if (auto textObject = typeinfo_cast<TextGameObject*>(m_targetObject)) {
					textObject->updateTextObject("image:" + utils::string::pathToString(path), false);
				}
			});
		} else {
			CustomizeObjectLayer::onSelectMode(sender);
		}
		setTextBtn();
	}
};

class $modify(MyTextGameObject, TextGameObject) {
	struct Fields {
		Ref<LazySprite> m_spr;
	};

	void onImageFail() {
		for (auto child : CCArrayExt<CCNode*>(getChildren())) {
			child->setVisible(false);
		}
		auto node = CCNodeRGBA::create();
		node->setAnchorPoint({0.5f, 0.5f});
		node->ignoreAnchorPointForPosition(false);
		node->setID("error-node"_spr);

		node->setCascadeColorEnabled(true);
		node->setCascadeOpacityEnabled(true);

		auto spr = CCSprite::createWithSpriteFrameName("image-btn.png"_spr);
		node->addChild(spr);

		node->setContentSize(spr->getContentSize());
		node->setPosition(node->getContentSize()/2);
		spr->setPosition(node->getContentSize()/2);

		auto label = CCLabelBMFont::create("Image Not Found", "chatFont.fnt");
		label->setAnchorPoint({0.5f, 0.f});
		label->setPositionX(node->getContentWidth()/2);
		label->setPositionY(node->getContentHeight() + 3);

		node->addChild(label);
		addChild(node);
		
		setContentSize(node->getContentSize());
		m_width = getContentWidth();
		m_height = getContentHeight();
		updateOrientedBox();
	}

	void setAttributes() {
		log::info("set attr");
		auto fields = m_fields.self();
		if (auto node = getChildByID("error-node"_spr)) {
			node->removeFromParent();
		}
		setContentSize(fields->m_spr->getContentSize());
		fields->m_spr->setPosition(getContentSize()/2);
		fields->m_spr->setColor(getColor());
		fields->m_spr->setOpacity(getOpacity());
		m_width = getContentWidth();
		m_height = getContentHeight();
		updateOrientedBox();
	}

	void setupCustomSprite() {
		auto fields = m_fields.self();
		if (fields->m_spr) fields->m_spr->removeFromParent();
		if (LevelEditorLayer::get()) {
			m_hasSpecialChild = true;
		}

		if (auto node = getChildByID("error-node"_spr)) {
			node->removeFromParent();
		}

		auto pair = splitIntoPair(m_text);
		if (pair.first == "image") {
			if (!LevelEditorLayer::get()) {
				updateTextObject("[Path Hidden, Delete Object Before Upload!]", false);
				return;
			}
			else {
				for (auto child : CCArrayExt<CCNode*>(getChildren())) {
					child->setVisible(false);
				}
			}
		}
		else return;

		if (pair.second.empty()) return;

		auto path = std::filesystem::path(pair.second);
		if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {

			auto fields = m_fields.self();
			fields->m_spr = LazySprite::create({60, 60}, true);
			fields->m_spr->setZOrder(1);
			fields->m_spr->setPosition(getContentSize()/2);
			addChild(fields->m_spr);

			fields->m_spr->setLoadCallback([this](Result<> res) {
				if (res) setAttributes();
				else {
					for (auto child : CCArrayExt<CCNode*>(getChildren())) {
						child->setVisible(true);
					}
					auto fields = m_fields.self();
					fields->m_spr->removeFromParent();
					fields->m_spr = nullptr;
					onImageFail();
				}
			});

			fields->m_spr->loadFromFile(path, LazySprite::Format::kFmtUnKnown, true);
		}
		else {
			onImageFail();
		}
	}

    void customObjectSetup(gd::vector<gd::string>& p0, gd::vector<void*>& p1) {
		TextGameObject::customObjectSetup(p0, p1);
		setupCustomSprite();
	}

	void updateTextObject(gd::string p0, bool p1) {
		TextGameObject::updateTextObject(p0, p1);
		if (!LevelEditorLayer::get()) return;
		setupCustomSprite();
	}
};

class $modify(MyEditorUI, EditorUI) {

	void reloadButtonBar(EditButtonBar* buttonBar) {
		auto rows = GameManager::get()->getIntGameVariable("0049");
		auto cols = GameManager::get()->getIntGameVariable("0050");
		buttonBar->reloadItems(rows, cols);
	}

	void createMoveMenu() {
		EditorUI::createMoveMenu();

		auto btn = getSpriteButton("image-btn.png"_spr, menu_selector(MyEditorUI::onImport), nullptr, 0.9f);
		btn->setID("reference-import"_spr);
		m_editButtonBar->m_buttonArray->addObject(btn);
		reloadButtonBar(m_editButtonBar);
	}

	void onImport(CCObject* sender) {
		utils::file::FilePickOptions options;
		options.filters.push_back({"Images", {"*.png", "*.apng", "*.jpg", "*.jpeg", "*.jfif", "*.pjpeg", "*.pjp", "*.webp", "*.gif", "*.bmp", "*.jxl"}});

		utils::file::pick(file::PickMode::OpenFile, options).listen([this](Result<std::filesystem::path>* result) {
			if (!*result) return;
			auto path = result->unwrap();
			float posX = 0;
			float posY = 0;
			if (m_selectedObject) {
				posX = m_selectedObject->getPositionX();
				posY = m_selectedObject->getPositionY();
			}
			else {
				auto winSize = CCDirector::get()->getWinSize();
				CCPoint localPosAR = m_editorLayer->m_objectLayer->convertToNodeSpaceAR(winSize/2);
				posX = localPosAR.x;
				posY = localPosAR.y - m_toolbarHeight/2;
			}
			std::string obj = fmt::format("1,914,2,{},3,{},31,{}", posX, posY, utils::base64::encode("image:" + utils::string::pathToString(path)));
			pasteObjects(obj, true, true);
			updateButtons();
			updateObjectInfoLabel();
		});
	}
};